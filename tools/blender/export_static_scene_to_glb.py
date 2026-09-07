"""Export the active Blender scene as a static GLB and MToon sidecar.

Run from Blender's Text Editor. The script exports renderable mesh objects and
their parent hierarchy, embeds supported Principled textures in the GLB, and
writes <name>.mtoon.json beside it for StylizedRenderer.
"""

import json
from pathlib import Path

import bpy


# Empty uses the saved .blend path with .glb as its extension.
OUTPUT_PATH = ""

# Hidden objects are normally authoring helpers and are excluded by default.
EXPORT_HIDDEN_OBJECTS = False

# Per-material overrides keyed by the original Blender material name. Every
# field is optional and replaces the corresponding generated sidecar value.
MATERIAL_OVERRIDES: dict[str, dict] = {}

MATERIAL_SUFFIX = "__stylized_scene"
ALPHA_CUTOFF = 0.5


def log(message: str) -> None:
    print(f"[stylized-scene-export] {message}", flush=True)


def output_paths() -> tuple[Path, Path]:
    if OUTPUT_PATH:
        glb_path = Path(bpy.path.abspath(OUTPUT_PATH))
    else:
        if not bpy.data.filepath:
            raise RuntimeError(
                "Save the .blend file or configure OUTPUT_PATH."
            )
        glb_path = Path(bpy.data.filepath).with_suffix(".glb")

    if glb_path.suffix.casefold() != ".glb":
        glb_path = glb_path.with_suffix(".glb")

    return glb_path, glb_path.with_suffix(".mtoon.json")


def is_mmd_helper(obj: bpy.types.Object) -> bool:
    mmd_type = str(getattr(obj, "mmd_type", "NONE")).upper()
    return mmd_type in {
        "RIGID_BODY",
        "JOINT",
        "TEMPORARY",
        "TRACK_TARGET",
        "NON_COLLISION_CONSTRAINT",
    }


def export_objects(scene: bpy.types.Scene) -> list[bpy.types.Object]:
    meshes = [
        obj
        for obj in scene.objects
        if obj.type == "MESH"
        and not is_mmd_helper(obj)
        and (
            EXPORT_HIDDEN_OBJECTS
            or (not obj.hide_get() and not obj.hide_render)
        )
    ]
    if not meshes:
        raise RuntimeError("The active scene has no exportable mesh objects.")

    objects = set(meshes)
    for mesh in meshes:
        parent = mesh.parent
        while parent is not None:
            if parent.type in {"EMPTY", "ARMATURE"}:
                objects.add(parent)
            parent = parent.parent

    return sorted(objects, key=lambda obj: obj.name)


def principled_node(
    material: bpy.types.Material,
) -> bpy.types.Node | None:
    if not material.use_nodes or material.node_tree is None:
        return None
    return next(
        (
            node
            for node in material.node_tree.nodes
            if node.type == "BSDF_PRINCIPLED"
        ),
        None,
    )


def linked_image(
    socket: bpy.types.NodeSocket | None,
) -> bpy.types.Image | None:
    if socket is None or not socket.is_linked:
        return None

    pending = [link.from_node for link in socket.links]
    visited: set[bpy.types.Node] = set()
    while pending:
        node = pending.pop()
        if node in visited:
            continue
        visited.add(node)
        if node.type == "TEX_IMAGE" and node.image is not None:
            return node.image
        pending.extend(
            link.from_node
            for input_socket in node.inputs
            for link in input_socket.links
        )
    return None


def material_source(
    material: bpy.types.Material,
) -> dict:
    principled = principled_node(material)
    base_socket = (
        principled.inputs.get("Base Color")
        if principled is not None
        else None
    )
    alpha_socket = (
        principled.inputs.get("Alpha")
        if principled is not None
        else None
    )
    normal_socket = (
        principled.inputs.get("Normal")
        if principled is not None
        else None
    )

    base_color = (
        tuple(float(value) for value in base_socket.default_value)
        if base_socket is not None
        else tuple(float(value) for value in material.diffuse_color)
    )
    alpha = (
        float(alpha_socket.default_value)
        if alpha_socket is not None
        else base_color[3]
    )

    alpha_mode = "OPAQUE"
    blend_method = str(getattr(material, "blend_method", "OPAQUE"))
    surface_method = str(
        getattr(material, "surface_render_method", "DITHERED")
    )
    if blend_method in {"BLEND", "HASHED"} or surface_method == "DITHERED":
        alpha_mode = "BLEND" if alpha < 0.999 else "OPAQUE"
    if blend_method == "CLIP":
        alpha_mode = "MASK"
    if alpha < 0.999:
        alpha_mode = "BLEND"

    return {
        "baseColor": (
            base_color[0],
            base_color[1],
            base_color[2],
            alpha,
        ),
        "baseImage": linked_image(base_socket),
        "normalImage": linked_image(normal_socket),
        "alphaMode": alpha_mode,
        "doubleSided": not material.use_backface_culling,
    }


def configure_alpha(
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
        log(f"warning: could not configure alpha for {material.name}")


def temporary_material(
    source: bpy.types.Material,
) -> tuple[bpy.types.Material, dict]:
    captured = material_source(source)
    result = bpy.data.materials.new(f"{source.name}{MATERIAL_SUFFIX}")
    result.use_nodes = True
    result.diffuse_color = captured["baseColor"]
    result.use_backface_culling = not captured["doubleSided"]
    result["stylized_source_name"] = source.name
    result["stylized_alpha_mode"] = captured["alphaMode"]

    nodes = result.node_tree.nodes
    links = result.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (500.0, 0.0)
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.location = (180.0, 0.0)
    shader.inputs["Base Color"].default_value = captured["baseColor"]
    shader.inputs["Alpha"].default_value = captured["baseColor"][3]
    shader.inputs["Metallic"].default_value = 0.0
    shader.inputs["Roughness"].default_value = 1.0
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    base_image = captured["baseImage"]
    if base_image is not None:
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = base_image
        texture.location = (-420.0, 100.0)
        links.new(texture.outputs["Color"], shader.inputs["Base Color"])
        if captured["alphaMode"] != "OPAQUE":
            links.new(texture.outputs["Alpha"], shader.inputs["Alpha"])

    normal_image = captured["normalImage"]
    if normal_image is not None:
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = normal_image
        texture.location = (-420.0, -180.0)
        normal_map = nodes.new("ShaderNodeNormalMap")
        normal_map.location = (-100.0, -180.0)
        links.new(texture.outputs["Color"], normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])

    configure_alpha(result, captured["alphaMode"])
    return result, captured


def install_materials(
    meshes: list[bpy.types.Object],
) -> tuple[dict, list[bpy.types.Material], list[dict]]:
    original_slots: dict[bpy.types.Mesh, list] = {}
    temporary: list[bpy.types.Material] = []
    records: list[dict] = []
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
                replacement, captured = temporary_material(source)
                converted[source] = replacement
                temporary.append(replacement)
                records.append(sidecar_record(source, replacement, captured))
            mesh.materials[index] = replacement

    return original_slots, temporary, records


def restore_materials(original_slots: dict, temporary: list) -> None:
    for mesh, materials in original_slots.items():
        if len(mesh.materials) != len(materials):
            raise RuntimeError("Material slots changed during GLB export.")
        for index, material in enumerate(materials):
            mesh.materials[index] = material
    for material in temporary:
        bpy.data.materials.remove(material)


def sidecar_record(
    source: bpy.types.Material,
    exported: bpy.types.Material,
    captured: dict,
) -> dict:
    base = captured["baseColor"]
    record = {
        "name": exported.name,
        "baseColorFactor": list(base),
        "shadeColor": [base[0] * 0.35, base[1] * 0.35, base[2] * 0.35],
        "shadingShift": 0.0,
        "shadingShiftTextureScale": 1.0,
        "shadingToony": 0.9,
        "normalScale": 1.0,
        "surfaceOffset": 0.0,
        "shadowNormalInfluence": 0.0,
        "receiveShadow": True,
        "shadowCutoffEnabled": captured["alphaMode"] == "MASK",
        "shadowCutoff": ALPHA_CUTOFF,
        "giEqualization": 0.75,
        "screenOutline": {
            "enabled": True,
            "depthEnabled": True,
            "normalEnabled": True,
            "detectSelfDepth": True,
            "detectSelfNormal": True,
        },
        "outline": {
            "enabled": False,
            "widthMode": "world",
            "width": 1.0,
            "color": [0.0, 0.0, 0.0],
            "lightingMix": 0.0,
        },
        "textures": {
            key: ""
            for key in (
                "baseColor", "shade", "toonRamp", "normal",
                "shadingShift", "matcap", "rimMask", "emission",
                "outlineWidthMask", "occlusion", "specular",
            )
        },
    }
    record.update(MATERIAL_OVERRIDES.get(source.name, {}))
    return record


def select_objects(objects: list[bpy.types.Object]) -> tuple[list, object]:
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = next(
        obj for obj in objects if obj.type == "MESH"
    )
    return selected, active


def restore_selection(selected: list, active: object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in selected:
        if obj.name in bpy.context.scene.objects:
            obj.select_set(True)
    if active is not None and active.name in bpy.context.scene.objects:
        bpy.context.view_layer.objects.active = active


def export_glb(destination: Path) -> None:
    options = {
        "filepath": str(destination),
        "check_existing": False,
        "export_format": "GLB",
        "use_selection": True,
        "export_cameras": False,
        "export_lights": False,
        "export_extras": False,
        "export_yup": True,
        "export_apply": True,
        "export_animations": False,
        "export_skins": False,
        "export_morph": False,
        "export_materials": "EXPORT",
        "export_texcoords": True,
        "export_normals": True,
        "export_tangents": False,
        "export_draco_mesh_compression_enable": False,
    }
    supported = {
        prop.identifier
        for prop in bpy.ops.export_scene.gltf.get_rna_type().properties
    }
    result = bpy.ops.export_scene.gltf(**{
        key: value for key, value in options.items() if key in supported
    })
    if "FINISHED" not in result:
        raise RuntimeError(f"Blender glTF export failed: {result}")


def main() -> None:
    scene = bpy.context.scene
    objects = export_objects(scene)
    meshes = [obj for obj in objects if obj.type == "MESH"]
    glb_path, sidecar_path = output_paths()
    glb_path.parent.mkdir(parents=True, exist_ok=True)

    original_slots = {}
    temporary = []
    selected = []
    active = None
    try:
        original_slots, temporary, records = install_materials(meshes)
        selected, active = select_objects(objects)
        export_glb(glb_path)
        sidecar_path.write_text(
            json.dumps(
                {"version": 3, "materials": records},
                ensure_ascii=False,
                indent=2,
            ),
            encoding="utf-8",
        )
    finally:
        if selected or active is not None:
            restore_selection(selected, active)
        if original_slots or temporary:
            restore_materials(original_slots, temporary)

    log(f"objects: {len(objects)}; meshes: {len(meshes)}")
    log(f"scene GLB: {glb_path}")
    log(f"MToon sidecar: {sidecar_path}")


if __name__ == "__main__":
    main()
