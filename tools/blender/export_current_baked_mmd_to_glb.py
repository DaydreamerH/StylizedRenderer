"""Export the currently loaded baked MMD character to a single GLB file.

Run this script from Blender after ``bake_current_mmd_animation.py`` finishes.
Only the baked armature and meshes bound to it are exported. MMD root objects,
rigid bodies, joints, cameras, lights, and temporary controls are excluded.

MMD materials are temporarily replaced with simple Principled BSDF materials
so their base-color textures can be embedded in glTF. The original Blender
materials are restored immediately after export.
"""

import json
from pathlib import Path
import struct

import bpy


OUTPUT_PATH = ""
REQUIRE_BAKED_ARMATURE = False
EXPORT_ONLY_DEFORM_BONES = False
EXPORT_MORPH_NORMALS = False

# The inverted-hull outline pass reads this baked smooth-normal field from
# glTF COLOR_0.  Export by explicit name rather than relying on a material
# node, because temporary export materials intentionally do not consume it.
OUTLINE_NORMAL_COLOR_ATTRIBUTE = "outline_normal"

ALPHA_CUTOFF = 0.5
ALPHA_OPAQUE_THRESHOLD = 0.999
MAX_ALPHA_SAMPLES = 4096

BLEND_MATERIAL_NAME_TOKENS = (
    "glass",
    "translucent",
    "transparent",
    "sheer",
    "veil",
    "window",
    "lens",
    # MMD character eyes commonly place highlights and soft shading in a
    # separate alpha-blended layer above the left/right iris materials.
    "eyes+",
    "eye+",
    "eyeblend",
    "ガラス",
    "半透明",
    "透明",
    "玻璃",
)

CUTOUT_MATERIAL_NAME_TOKENS = (
    "stocking",
    "tights",
    "pantyhose",
    "hosiery",
    "丝袜",
    "连裤袜",
    "過膝",
    "タイツ",
    "ストッキング",
)

# Optional exact-name overrides for models whose material names do not reveal
# their intended transparency. Valid values are OPAQUE, MASK and BLEND.
ALPHA_MODE_OVERRIDES: dict[str, str] = {}

EXPORT_MATERIAL_SUFFIX = "__stylized_gltf"

GLB_MAGIC = b"glTF"
GLB_VERSION = 2
GLB_JSON_CHUNK = 0x4E4F534A
GLB_BINARY_CHUNK = 0x004E4942

COMPONENT_BYTE_SIZES = {
    5120: 1,
    5121: 1,
    5122: 2,
    5123: 2,
    5125: 4,
    5126: 4,
}

ACCESSOR_COMPONENT_COUNTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


def log(message: str) -> None:
    print(f"[stylized-export] {message}", flush=True)


def find_armature() -> bpy.types.Object:
    active = bpy.context.view_layer.objects.active
    if active is not None and active.type == "ARMATURE":
        return active

    baked = [
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "ARMATURE"
        and obj.get("stylized_pose_baked", False)
    ]
    if len(baked) == 1:
        return baked[0]

    armatures = [
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "ARMATURE"
    ]
    if not armatures:
        raise RuntimeError("No armature exists in the current scene.")

    return max(armatures, key=lambda obj: len(obj.data.bones))


def find_bound_meshes(armature: bpy.types.Object) -> list[bpy.types.Object]:
    return [
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "MESH"
        and any(
            modifier.type == "ARMATURE"
            and modifier.object == armature
            for modifier in obj.modifiers
        )
    ]


def find_base_color_image(
    material: bpy.types.Material,
) -> bpy.types.Image | None:
    if material.node_tree is None:
        return None

    nodes = material.node_tree.nodes
    mmd_base_texture = nodes.get("mmd_base_tex")
    if (
        mmd_base_texture is not None
        and mmd_base_texture.type == "TEX_IMAGE"
        and mmd_base_texture.image is not None
    ):
        return mmd_base_texture.image

    return next(
        (
            node.image
            for node in nodes
            if node.type == "TEX_IMAGE"
            and node.image is not None
            and "toon" not in node.name.lower()
            and "sphere" not in node.name.lower()
        ),
        None,
    )


def mmd_base_color(
    material: bpy.types.Material,
) -> tuple[float, float, float, float]:
    mmd_material = getattr(material, "mmd_material", None)
    if mmd_material is not None:
        color = tuple(mmd_material.diffuse_color)
        alpha = float(mmd_material.alpha)
        return color[0], color[1], color[2], alpha

    color = tuple(material.diffuse_color)
    return color[0], color[1], color[2], color[3]


def material_name_requests_blend(
    material: bpy.types.Material,
) -> bool:
    name = material.name.casefold()
    return any(
        token.casefold() in name
        for token in BLEND_MATERIAL_NAME_TOKENS
    )


def material_name_requests_cutout(
    material: bpy.types.Material,
) -> bool:
    name = material.name.casefold()
    return any(
        token.casefold() in name
        for token in CUTOUT_MATERIAL_NAME_TOKENS
    )


def alpha_mode_override(material: bpy.types.Material) -> str | None:
    name = material.name
    source_name = (
        name[:-len(EXPORT_MATERIAL_SUFFIX)]
        if name.endswith(EXPORT_MATERIAL_SUFFIX)
        else name
    )

    return ALPHA_MODE_OVERRIDES.get(
        name,
        ALPHA_MODE_OVERRIDES.get(source_name),
    )


def image_uses_alpha(image: bpy.types.Image | None) -> bool:
    if image is None or image.channels < 4:
        return False

    width, height = image.size
    pixel_count = int(width) * int(height)
    if pixel_count <= 0:
        return False

    try:
        pixels = image.pixels
        sample_count = min(pixel_count, MAX_ALPHA_SAMPLES)
        sample_step = max(1, pixel_count // sample_count)
        sampled = 0

        for pixel_index in range(0, pixel_count, sample_step):
            alpha_index = pixel_index * image.channels + 3
            if float(pixels[alpha_index]) < ALPHA_OPAQUE_THRESHOLD:
                return True

            sampled += 1
            if sampled >= sample_count:
                break
    except (IndexError, RuntimeError, TypeError, ValueError) as error:
        log(
            f"warning: could not inspect alpha channel of "
            f"{image.name}: {error}"
        )

    return False


def classify_alpha_mode(
    material: bpy.types.Material,
    image: bpy.types.Image | None,
    source_alpha: float,
) -> str:
    override = alpha_mode_override(material)
    if override is not None:
        normalized = override.upper()
        if normalized not in {"OPAQUE", "MASK", "BLEND"}:
            raise ValueError(
                f"Invalid alpha override for {material.name}: {override}"
            )
        return normalized

    # Stockings and similar garments commonly encode tears as sparse alpha
    # cutouts. Regular pixel sampling can miss those small transparent areas,
    # so classify them from their material name before inspecting the image.
    if material_name_requests_cutout(material):
        return "MASK"

    if source_alpha < ALPHA_OPAQUE_THRESHOLD:
        return "BLEND"

    if not image_uses_alpha(image):
        return "OPAQUE"

    if material_name_requests_blend(material):
        return "BLEND"

    # MMD textures with an alpha channel are normally cutouts such as hair,
    # eyelashes, brows and decals. Treat them as MASK unless the material
    # explicitly describes a genuinely translucent surface.
    return "MASK"


def configure_blender_alpha_mode(
    material: bpy.types.Material,
    alpha_mode: str,
) -> None:
    if alpha_mode == "OPAQUE":
        return

    try:
        if hasattr(material, "surface_render_method"):
            material.surface_render_method = "DITHERED"
        elif hasattr(material, "blend_method"):
            material.blend_method = (
                "CLIP" if alpha_mode == "MASK" else "HASHED"
            )

        if hasattr(material, "alpha_threshold"):
            material.alpha_threshold = ALPHA_CUTOFF
    except (TypeError, ValueError):
        log(
            "warning: Blender could not set transparent render mode "
            f"for material {material.name}."
        )


def make_gltf_material(
    source: bpy.types.Material,
) -> bpy.types.Material:
    result = bpy.data.materials.new(
        name=f"{source.name}__stylized_gltf"
    )
    result.use_nodes = True
    source_color = mmd_base_color(source)
    base_color = (
        source_color[0],
        source_color[1],
        source_color[2],
        source_color[3],
    )
    result.diffuse_color = base_color

    image = find_base_color_image(source)
    alpha_mode = classify_alpha_mode(
        source,
        image,
        source_color[3],
    )
    result["stylized_alpha_mode"] = alpha_mode
    result["stylized_alpha_cutoff"] = ALPHA_CUTOFF

    mmd_material = getattr(source, "mmd_material", None)
    if mmd_material is not None:
        result.use_backface_culling = not mmd_material.is_double_sided
    else:
        result.use_backface_culling = source.use_backface_culling

    nodes = result.node_tree.nodes
    links = result.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (420.0, 0.0)

    principled = nodes.new("ShaderNodeBsdfPrincipled")
    principled.location = (100.0, 0.0)
    principled.inputs["Base Color"].default_value = base_color
    principled.inputs["Metallic"].default_value = 0.0
    principled.inputs["Roughness"].default_value = 1.0
    principled.inputs["Alpha"].default_value = source_color[3]
    links.new(principled.outputs["BSDF"], output.inputs["Surface"])

    if image is not None:
        texture = nodes.new("ShaderNodeTexImage")
        texture.location = (-240.0, 0.0)
        texture.image = image
        links.new(texture.outputs["Color"], principled.inputs["Base Color"])

        if alpha_mode != "OPAQUE":
            links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])

    configure_blender_alpha_mode(result, alpha_mode)

    return result


def install_temporary_materials(
    meshes: list[bpy.types.Object],
) -> tuple[
    dict[bpy.types.Mesh, list[bpy.types.Material | None]],
    list[bpy.types.Material],
]:
    original_slots: dict[
        bpy.types.Mesh,
        list[bpy.types.Material | None],
    ] = {}
    temporary_materials: list[bpy.types.Material] = []
    converted: dict[bpy.types.Material, bpy.types.Material] = {}

    for obj in meshes:
        mesh = obj.data
        if mesh in original_slots:
            continue

        original_slots[mesh] = list(mesh.materials)
        for index, source in enumerate(original_slots[mesh]):
            if source is None:
                continue

            replacement = converted.get(source)
            if replacement is None:
                replacement = make_gltf_material(source)
                converted[source] = replacement
                temporary_materials.append(replacement)

            mesh.materials[index] = replacement

    return original_slots, temporary_materials


def restore_materials(
    original_slots: dict[
        bpy.types.Mesh,
        list[bpy.types.Material | None],
    ],
    temporary_materials: list[bpy.types.Material],
) -> None:
    for mesh, materials in original_slots.items():
        if len(mesh.materials) != len(materials):
            raise RuntimeError(
                "Temporary material slot count changed during GLB export."
            )

        # Do not clear the material slots. Blender resets every polygon's
        # material_index to zero when the slots are cleared, which makes a
        # subsequent export apply material slot 0 to the entire MMD mesh.
        for index, material in enumerate(materials):
            mesh.materials[index] = material

    for material in temporary_materials:
        bpy.data.materials.remove(material)


def exported_alpha_modes(
    materials: list[bpy.types.Material],
) -> dict[str, tuple[str, float, tuple[float, float, float, float]]]:
    return {
        material.name: (
            str(material.get("stylized_alpha_mode", "OPAQUE")),
            float(
                material.get(
                    "stylized_alpha_cutoff",
                    ALPHA_CUTOFF,
                )
            ),
            tuple(float(value) for value in material.diffuse_color),
        )
        for material in materials
    }


def read_glb_chunks(
    data: bytes,
) -> list[tuple[int, bytes]]:
    if len(data) < 12:
        raise RuntimeError("Exported GLB is smaller than its header.")

    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    if magic != GLB_MAGIC or version != GLB_VERSION:
        raise RuntimeError("Exported file is not a GLB 2.0 container.")
    if total_length != len(data):
        raise RuntimeError(
            "Exported GLB header length does not match the file size."
        )

    chunks: list[tuple[int, bytes]] = []
    offset = 12
    while offset < total_length:
        if offset + 8 > total_length:
            raise RuntimeError("Exported GLB has a truncated chunk header.")

        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_length
        if chunk_end > total_length:
            raise RuntimeError("Exported GLB has a truncated chunk body.")

        chunks.append((chunk_type, data[chunk_start:chunk_end]))
        offset = chunk_end

    return chunks


def write_glb_chunks(
    destination: Path,
    chunks: list[tuple[int, bytes]],
) -> None:
    body = bytearray()
    for chunk_type, payload in chunks:
        body.extend(struct.pack("<II", len(payload), chunk_type))
        body.extend(payload)

    output = struct.pack(
        "<4sII",
        GLB_MAGIC,
        GLB_VERSION,
        12 + len(body),
    ) + body

    temporary_path = destination.with_name(
        f"{destination.name}.stylized.tmp"
    )
    temporary_path.write_bytes(output)
    temporary_path.replace(destination)


def patch_glb_alpha_modes(
    destination: Path,
    materials: list[bpy.types.Material],
) -> None:
    settings = exported_alpha_modes(materials)
    chunks = read_glb_chunks(destination.read_bytes())

    json_indices = [
        index
        for index, (chunk_type, _) in enumerate(chunks)
        if chunk_type == GLB_JSON_CHUNK
    ]
    if len(json_indices) != 1:
        raise RuntimeError(
            "Exported GLB does not contain exactly one JSON chunk."
        )

    json_index = json_indices[0]
    json_payload = chunks[json_index][1].rstrip(b" \t\r\n\0")
    document = json.loads(json_payload.decode("utf-8"))

    binary_indices = [
        index
        for index, (chunk_type, _) in enumerate(chunks)
        if chunk_type == GLB_BINARY_CHUNK
    ]
    if len(binary_indices) != 1:
        raise RuntimeError(
            "Exported GLB does not contain exactly one binary chunk."
        )

    binary_index = binary_indices[0]
    binary_payload = bytearray(chunks[binary_index][1])
    buffers = document.get("buffers", [])
    if len(buffers) != 1:
        raise RuntimeError(
            "The exporter only supports a single-buffer GLB."
        )

    logical_binary_length = int(buffers[0].get("byteLength", 0))
    if logical_binary_length > len(binary_payload):
        raise RuntimeError("GLB binary chunk is shorter than its buffer.")
    del binary_payload[logical_binary_length:]

    buffer_views = document.setdefault("bufferViews", [])
    shared_zero_views: dict[int, int] = {}
    repaired_accessors = 0

    for accessor in document.get("accessors", []):
        if "bufferView" in accessor or "sparse" in accessor:
            continue

        component_size = COMPONENT_BYTE_SIZES.get(
            int(accessor.get("componentType", 0))
        )
        component_count = ACCESSOR_COMPONENT_COUNTS.get(
            accessor.get("type")
        )
        if component_size is None or component_count is None:
            raise RuntimeError(
                "Cannot materialize an unsupported implicit-zero accessor."
            )

        byte_length = (
            int(accessor.get("count", 0))
            * component_size
            * component_count
        )
        view_index = shared_zero_views.get(byte_length)
        if view_index is None:
            padding = (-len(binary_payload)) % 4
            if padding:
                binary_payload.extend(b"\0" * padding)

            byte_offset = len(binary_payload)
            binary_payload.extend(b"\0" * byte_length)
            view_index = len(buffer_views)
            buffer_views.append(
                {
                    "buffer": 0,
                    "byteOffset": byte_offset,
                    "byteLength": byte_length,
                }
            )
            shared_zero_views[byte_length] = view_index

        accessor["bufferView"] = view_index
        repaired_accessors += 1

    buffers[0]["byteLength"] = len(binary_payload)
    binary_payload.extend(b"\0" * ((-len(binary_payload)) % 4))
    chunks[binary_index] = (GLB_BINARY_CHUNK, bytes(binary_payload))

    mode_counts = {"OPAQUE": 0, "MASK": 0, "BLEND": 0}
    matched_names: set[str] = set()
    for material in document.get("materials", []):
        name = material.get("name")
        setting = settings.get(name)
        if setting is None:
            continue

        alpha_mode, alpha_cutoff, base_color_factor = setting
        matched_names.add(name)
        mode_counts[alpha_mode] += 1

        # Once an image is linked to Principled Base Color/Alpha, Blender's
        # glTF exporter can omit the original MMD diffuse and opacity factors.
        # glTF defines baseColorFactor as a multiplier of the texture, so put
        # the captured PMX factor back explicitly. This is essential for eye
        # highlights and other translucent overlay materials.
        pbr = material.setdefault("pbrMetallicRoughness", {})
        pbr["baseColorFactor"] = list(base_color_factor)

        if alpha_mode == "OPAQUE":
            material.pop("alphaMode", None)
            material.pop("alphaCutoff", None)
        elif alpha_mode == "MASK":
            material["alphaMode"] = "MASK"
            material["alphaCutoff"] = alpha_cutoff
        elif alpha_mode == "BLEND":
            material["alphaMode"] = "BLEND"
            material.pop("alphaCutoff", None)
        else:
            raise RuntimeError(
                f"Unsupported alpha mode for {name}: {alpha_mode}"
            )

    encoded_json = json.dumps(
        document,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    encoded_json += b" " * ((-len(encoded_json)) % 4)
    chunks[json_index] = (GLB_JSON_CHUNK, encoded_json)
    write_glb_chunks(destination, chunks)

    unmatched = sorted(set(settings) - matched_names)
    log(
        "alpha modes: "
        f"opaque={mode_counts['OPAQUE']}, "
        f"mask={mode_counts['MASK']}, "
        f"blend={mode_counts['BLEND']}"
    )
    if unmatched:
        log(
            "warning: GLB omitted temporary materials: "
            + ", ".join(unmatched)
        )
    if repaired_accessors:
        log(
            "materialized implicit-zero accessors for Assimp: "
            f"{repaired_accessors}"
        )


def select_export_objects(
    armature: bpy.types.Object,
    meshes: list[bpy.types.Object],
) -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    bpy.ops.object.select_all(action="DESELECT")

    for obj in [armature, *meshes]:
        obj.hide_set(False)
        obj.hide_viewport = False
        obj.select_set(True)

    bpy.context.view_layer.objects.active = armature


def count_morph_targets(meshes: list[bpy.types.Object]) -> int:
    count = 0
    for obj in meshes:
        shape_keys = obj.data.shape_keys
        if shape_keys is not None:
            count += max(0, len(shape_keys.key_blocks) - 1)
    return count


def count_shape_key_actions(meshes: list[bpy.types.Object]) -> int:
    count = 0
    for obj in meshes:
        shape_keys = obj.data.shape_keys
        animation_data = (
            shape_keys.animation_data
            if shape_keys is not None
            else None
        )
        if animation_data is not None and animation_data.action is not None:
            count += 1
    return count


def meshes_with_outline_normal(
    meshes: list[bpy.types.Object],
) -> list[bpy.types.Object]:
    return [
        mesh
        for mesh in meshes
        if mesh.data.color_attributes.get(
            OUTLINE_NORMAL_COLOR_ATTRIBUTE)
        is not None
    ]


def output_path() -> Path:
    if OUTPUT_PATH:
        return Path(bpy.path.abspath(OUTPUT_PATH))

    if not bpy.data.filepath:
        raise RuntimeError(
            "Save the baked .blend file before exporting GLB."
        )

    return Path(bpy.data.filepath).with_suffix(".glb")


def main() -> None:
    armature = find_armature()
    is_marked_baked = armature.get("stylized_pose_baked", False)
    if REQUIRE_BAKED_ARMATURE and not is_marked_baked:
        raise RuntimeError(
            "The selected armature was not produced by the bake script."
        )
    if not is_marked_baked:
        log(
            "warning: the armature has no stylized_pose_baked marker; "
            "exporting its active action anyway."
        )

    animation_data = armature.animation_data
    action = (
        animation_data.action
        if animation_data is not None
        else None
    )
    if action is None:
        raise RuntimeError("The armature has no active baked action.")

    meshes = find_bound_meshes(armature)
    if not meshes:
        raise RuntimeError("No skinned meshes are bound to the armature.")

    destination = output_path()
    destination.parent.mkdir(parents=True, exist_ok=True)

    log(f"armature: {armature.name}")
    log(f"action: {action.name}")
    log(f"skinned meshes: {len(meshes)}")
    log(f"morph targets: {count_morph_targets(meshes)}")
    log(f"active Shape Key actions: {count_shape_key_actions(meshes)}")
    outline_normal_meshes = meshes_with_outline_normal(meshes)
    vertex_color_export_options: dict[str, object]
    if outline_normal_meshes:
        vertex_color_export_options = {
            "export_vertex_color": "NAME",
            "export_vertex_color_name":
                OUTLINE_NORMAL_COLOR_ATTRIBUTE,
            "export_all_vertex_colors": False,
        }
        log(
            "exporting outline-normal color attribute from "
            f"{len(outline_normal_meshes)} mesh(es): "
            f"{OUTLINE_NORMAL_COLOR_ATTRIBUTE} -> COLOR_0"
        )
    else:
        # Blender rejects NAME mode when the requested Color Attribute does
        # not exist. Plain GLB exports must remain valid after a user removes
        # the experimental outline-normal data.
        vertex_color_export_options = {
            "export_vertex_color": "NONE",
            "export_all_vertex_colors": False,
        }
        log(
            "warning: no skinned mesh has the "
            f"'{OUTLINE_NORMAL_COLOR_ATTRIBUTE}' color attribute; "
            "the GLB will not contain baked outline normals."
        )
    log("cameras: excluded (use export_camera_glb.py)")

    select_export_objects(armature, meshes)
    original_slots, temporary_materials = install_temporary_materials(
        meshes
    )

    try:
        log(f"exporting GLB: {destination}")
        result = bpy.ops.export_scene.gltf(
            filepath=str(destination),
            check_existing=False,
            export_format="GLB",
            use_selection=True,
            use_visible=False,
            export_cameras=False,
            export_lights=False,
            export_extras=False,
            export_texcoords=True,
            export_normals=True,
            export_tangents=False,
            export_materials="EXPORT",
            export_yup=True,
            export_apply=False,
            export_draco_mesh_compression_enable=False,
            export_animations=True,
            export_animation_mode="ACTIVE_ACTIONS",
            export_frame_range=True,
            export_frame_step=1,
            # Physics bones are already keyed, but ordinary MMD leg motion
            # still depends on live IK constraints in the baked .blend.
            # glTF cannot carry those constraints, so sample the final
            # evaluated pose of every exported bone at each frame.
            export_force_sampling=True,
            export_nla_strips=False,
            export_def_bones=EXPORT_ONLY_DEFORM_BONES,
            export_leaf_bone=False,
            export_armature_object_remove=False,
            export_anim_single_armature=True,
            export_reset_pose_bones=True,
            export_current_frame=False,
            export_skins=True,
            export_influence_nb=4,
            export_all_influences=False,
            export_morph=True,
            export_morph_normal=EXPORT_MORPH_NORMALS,
            export_morph_tangent=False,
            export_morph_animation=True,
            export_morph_reset_sk_data=True,
            # Keep the Blender-generated Point-domain Float Color Attribute
            # even though the temporary export material does not use it.
            # This deliberately maps outline_normal to glTF COLOR_0 so the
            # renderer can consume one stable, explicit attribute stream.
            **vertex_color_export_options,
        )
        if "FINISHED" not in result:
            raise RuntimeError(f"GLB export failed: {result}")

        if not destination.is_file() or destination.stat().st_size == 0:
            raise RuntimeError(
                "Blender reported success but no GLB was written."
            )

        # Blender 5 exposes a single dithered surface mode and may serialize
        # cutout materials as BLEND. Patch the standard glTF material fields
        # after export so the renderer receives the intended queue semantics.
        patch_glb_alpha_modes(destination, temporary_materials)
    finally:
        restore_materials(original_slots, temporary_materials)

    if not destination.is_file() or destination.stat().st_size == 0:
        raise RuntimeError("Blender reported success but no GLB was written.")

    size_mib = destination.stat().st_size / (1024.0 * 1024.0)
    log(f"DONE: {destination} ({size_mib:.1f} MiB)")


if __name__ == "__main__":
    main()
