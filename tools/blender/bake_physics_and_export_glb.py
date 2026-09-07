"""One-click MMD physics bake and renderer-ready GLB export.

Import PMX and VMD with MMD Tools, verify the animation, save the .blend,
then execute this file from Blender's Scripting workspace or Python Console.

The script uses the two focused helpers beside it for the actual pose bake and
GLB export. It additionally repairs MMD materials and writes an MToon sidecar
from the original PMX material data.
"""

from importlib.util import module_from_spec, spec_from_file_location
import hashlib
import json
import os
from pathlib import Path
import shutil

import bpy


TOOLS_DIRECTORY = Path(r"D:\StylizedRenderer\tools\blender")
# Generic conversion must preserve the PMX-authored rigid-body graph. Muting a
# driver by a name heuristic can also disturb torso colliders and dependent
# hair/head chains, so this model-specific option is deliberately off.
DISABLE_CHEST_PHYSICS = False

# Written as escapes so the script is safe under every Windows code page.
CHEST_NAME_TOKENS = (
    "\u80f8",                    # chest
    "\u4e73",                    # breast
    "\u30d0\u30b9\u30c8",       # bust
    "\u304a\u3063\u3071\u3044", # oppai
    "breast",
    "bust",
    "boob",
    "oppai",
)


def log(message: str) -> None:
    print(f"[stylized-one-click] {message}", flush=True)


def load_helper(name: str, filename: str):
    path = TOOLS_DIRECTORY / filename
    if not path.is_file():
        raise RuntimeError(f"Missing Blender helper script: {path}")

    spec = spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load Blender helper script: {path}")

    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def unique_materials(
    meshes: list[bpy.types.Object],
) -> list[bpy.types.Material]:
    result: list[bpy.types.Material] = []
    seen: set[int] = set()

    for obj in meshes:
        for material in obj.data.materials:
            if material is None or material.as_pointer() in seen:
                continue
            seen.add(material.as_pointer())
            result.append(material)

    return result


def node_image(
    material: bpy.types.Material,
    node_name: str,
) -> bpy.types.Image | None:
    if material.node_tree is None:
        return None

    node = material.node_tree.nodes.get(node_name)
    if node is None or node.type != "TEX_IMAGE":
        return None
    return node.image


def absolute_image_path(image: bpy.types.Image | None) -> str:
    if image is None or not image.filepath:
        return ""
    return str(Path(bpy.path.abspath(image.filepath)).resolve())


def find_normal_map(base_color_path: str) -> str:
    """Find the conventional *_n texture shipped beside an MMD model."""

    if not base_color_path:
        return ""

    base_path = Path(base_color_path)
    stem = base_path.stem.casefold()
    expected_stems = {stem}

    for suffix in ("_da", "_d"):
        if stem.endswith(suffix):
            expected_stems.add(stem[: -len(suffix)] + "_n")

    search_roots = [
        base_path.parent / "normalmap",
        base_path.parent.parent / "normalmap",
    ]

    for root in search_roots:
        if not root.is_dir():
            continue

        candidates = {
            path.stem.casefold(): path
            for path in root.iterdir()
            if path.is_file()
            and path.suffix.casefold() in {".png", ".bmp", ".tga"}
        }

        for expected in expected_stems:
            candidate = candidates.get(expected)
            if candidate is not None:
                return str(candidate.resolve())

    return ""


def texture_search_roots(
    base_color_path: str,
    directory_name: str,
) -> list[Path]:
    if not base_color_path:
        return []

    base_path = Path(base_color_path)
    return [
        base_path.parent / directory_name,
        base_path.parent.parent / directory_name,
    ]


def base_texture_stem(base_color_path: str) -> str:
    stem = Path(base_color_path).stem.casefold()

    for suffix in ("_da", "_d"):
        if stem.endswith(suffix):
            return stem[: -len(suffix)]

    return stem


def find_companion_map(
    base_color_path: str,
    suffix: str,
) -> str:
    """Find an auxiliary map derived from the base texture name.

    Files such as ``body_d.png`` and ``body_rmo.png`` are paired by the
    shared ``body`` stem. A dated suffix after ``_rmo``/``_spc`` is allowed.
    """

    if not base_color_path:
        return ""

    expected_prefix = base_texture_stem(base_color_path) + suffix.casefold()

    for root in texture_search_roots(base_color_path, "normalmap"):
        if not root.is_dir():
            continue

        for path in sorted(root.iterdir()):
            if (
                path.is_file()
                and path.suffix.casefold() in {".png", ".bmp", ".tga"}
                and path.stem.casefold().startswith(expected_prefix)
            ):
                return str(path.resolve())

    return ""


def find_named_texture(
    base_color_path: str,
    directory_name: str,
    filename: str,
) -> str:
    for root in texture_search_roots(base_color_path, directory_name):
        candidate = root / filename
        if candidate.is_file():
            return str(candidate.resolve())

    return ""


def is_eye_matcap_material(material_name: str) -> bool:
    name = material_name.casefold()
    return "white" not in name and (
        name == "eyes"
        or name.startswith("eyes+")
        or name.startswith("eyered")
    )


def capture_pmx_materials(
    meshes: list[bpy.types.Object],
) -> list[dict]:
    """Capture PMX fields before MMD Tools cleans the node graphs."""

    captured: list[dict] = []

    for material in unique_materials(meshes):
        mmd = getattr(material, "mmd_material", None)

        if mmd is not None:
            diffuse = [*map(float, mmd.diffuse_color), float(mmd.alpha)]
            ambient = list(map(float, mmd.ambient_color))
            specular = list(map(float, mmd.specular_color))
            edge_color = list(map(float, mmd.edge_color[:3]))
            edge_enabled = bool(mmd.enabled_toon_edge)
            edge_width = float(mmd.edge_weight)
            sphere_mode = int(mmd.sphere_texture_type)
        else:
            diffuse = list(map(float, material.diffuse_color))
            ambient = [0.35, 0.35, 0.35]
            specular = [1.0, 1.0, 1.0]
            edge_color = [0.0, 0.0, 0.0]
            edge_enabled = False
            edge_width = 1.0
            sphere_mode = 0

        base_image = node_image(material, "mmd_base_tex")
        sphere_image = node_image(material, "mmd_sphere_tex")
        toon_image = node_image(material, "mmd_toon_tex")
        base_color_path = absolute_image_path(base_image)
        normal_path = find_normal_map(base_color_path)
        toon_ramp_path = absolute_image_path(toon_image)
        occlusion_path = find_companion_map(base_color_path, "_rmo")
        specular_path = find_companion_map(base_color_path, "_spc")
        matcap_path = (
            absolute_image_path(sphere_image)
            if sphere_mode in (1, 2)
            else ""
        )

        if not matcap_path and is_eye_matcap_material(material.name):
            matcap_path = find_named_texture(
                base_color_path,
                "spa",
                "eyes.png",
            )

        captured.append({
            # The GLB helper gives its temporary glTF material this name.
            "name": f"{material.name}__stylized_gltf",
            "baseColorFactor": diffuse,
            "shadeColor": ambient,
            "shadingShift": 0.0,
            "shadingShiftTextureScale": 1.0,
            "shadingToony": 0.9,
            "normalScale": 1.0,
            "giEqualization": 0.75,
            "occlusionStrength": 1.0,
            "specularColor": [1.0, 1.0, 1.0],
            "specularStrength": 1.0,
            "specularPower": 64.0,
            "matcapColor": specular,
            "matcapStrength": 0.35 if matcap_path else 0.0,
            "rimColor": [0.0, 0.0, 0.0],
            "rimFresnelPower": 5.0,
            "rimLift": 0.0,
            "rimLightingMix": 1.0,
            "emissionColor": [0.0, 0.0, 0.0],
            "emissionStrength": 1.0,
            "outline": {
                "enabled": edge_enabled,
                "widthMode": "screen",
                "width": max(0.0, min(edge_width, 4.0)),
                "color": edge_color,
                "lightingMix": 0.0,
            },
            "textures": {
                "baseColor": base_color_path,
                "shade": "",
                "toonRamp": toon_ramp_path,
                "normal": normal_path,
                "shadingShift": "",
                "matcap": matcap_path,
                "rimMask": "",
                "emission": "",
                "outlineWidthMask": "",
                "occlusion": occlusion_path,
                "specular": specular_path,
            },
        })

    return captured


def rigid_body_search_text(obj: bpy.types.Object) -> str:
    names = [obj.name]
    mmd_rigid = getattr(obj, "mmd_rigid", None)
    if mmd_rigid is not None:
        for attribute in ("name_j", "name_e", "bone"):
            value = getattr(mmd_rigid, attribute, "")
            if value:
                names.append(str(value))
    return " ".join(names).casefold()


def disable_chest_rigid_bodies(armature: bpy.types.Object) -> int:
    disabled = 0

    for obj in bpy.context.scene.objects:
        if obj.rigid_body is None:
            continue

        text = rigid_body_search_text(obj)
        if not any(token.casefold() in text for token in CHEST_NAME_TOKENS):
            continue

        mmd_rigid = getattr(obj, "mmd_rigid", None)
        bone_name = getattr(mmd_rigid, "bone", "")
        pose_bone = armature.pose.bones.get(bone_name) if bone_name else None
        if pose_bone is not None:
            for constraint in pose_bone.constraints:
                target = getattr(constraint, "target", None)
                target_is_rigid_body = (
                    target is not None
                    and (
                        getattr(target, "rigid_body", None) is not None
                        or getattr(target, "mmd_type", "NONE")
                        == "RIGID_BODY"
                    )
                )
                if (
                    target_is_rigid_body
                    or "rigid" in constraint.name.casefold()
                ):
                    constraint.mute = True

        obj["stylized_chest_physics_disabled"] = True
        disabled += 1

    return disabled


def relative_texture_path(path: str, directory: Path) -> str:
    if not path:
        return ""

    source = Path(path).resolve()

    if not source.is_file() and not source.suffix:
        for extension in (".png", ".jpg", ".jpeg", ".bmp", ".tga"):
            candidate = source.with_suffix(extension)
            if candidate.is_file():
                source = candidate
                break

    if not source.is_file():
        print(
            "[stylized-one-click] warning: texture is not a file: "
            f"{source}"
        )
        return ""

    try:
        return Path(os.path.relpath(source, directory)).as_posix()
    except ValueError:
        # Windows cannot express a relative path across drive letters.  The
        # sidecar intentionally accepts relative paths only, so preserve the
        # external texture beside the exported asset instead of writing an
        # unusable absolute path.
        texture_directory = directory / "external_textures"
        texture_directory.mkdir(parents=True, exist_ok=True)

        digest = hashlib.sha1(
            str(source).casefold().encode("utf-8")
        ).hexdigest()[:8]
        destination = texture_directory / (
            f"{source.stem}_{digest}{source.suffix}"
        )

        if not destination.exists():
            shutil.copy2(source, destination)

        return Path(
            os.path.relpath(destination, directory)
        ).as_posix()


def write_mtoon_sidecar(materials: list[dict], glb_path: Path) -> Path:
    sidecar_path = glb_path.with_suffix(".mtoon.json")

    for material in materials:
        textures = material["textures"]
        for key, path in textures.items():
            textures[key] = relative_texture_path(path, sidecar_path.parent)

    payload = {
        "version": 2,
        "materials": materials,
    }
    sidecar_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return sidecar_path


def main() -> None:
    if not bpy.data.filepath:
        raise RuntimeError("Save the current .blend file before running.")

    bake_helper = load_helper(
        "stylized_mmd_bake",
        "bake_current_mmd_animation.py",
    )
    export_helper = load_helper(
        "stylized_mmd_export",
        "export_current_baked_mmd_to_glb.py",
    )

    armature = bake_helper.find_armature()
    meshes = bake_helper.find_bound_meshes(armature)
    if not meshes:
        raise RuntimeError("No skinned meshes are bound to the armature.")

    log(f"armature: {armature.name}")
    log(f"bound meshes: {len(meshes)}")

    scene = bpy.context.scene
    morph_samples = bake_helper.sample_shape_key_animation(
        meshes,
        int(scene.frame_start),
        int(scene.frame_end),
    )
    pmx_materials = capture_pmx_materials(meshes)

    if DISABLE_CHEST_PHYSICS:
        raise RuntimeError(
            "DISABLE_CHEST_PHYSICS is no longer supported by the "
            "one-click workflow. Edit the PMX rigid bodies before baking."
        )

    log("baking rigid-body physics and evaluated pose...")
    bake_helper.main(captured_morph_samples=morph_samples)

    log("exporting armature, meshes, morphs and animation...")
    export_helper.REQUIRE_BAKED_ARMATURE = True
    export_helper.main()

    glb_path = Path(bpy.data.filepath).with_suffix(".glb")
    sidecar_path = write_mtoon_sidecar(pmx_materials, glb_path)
    log(f"MToon sidecar: {sidecar_path}")
    log("camera export: skipped (run export_camera_glb.py separately)")
    log("DONE")


if __name__ == "__main__":
    main()
