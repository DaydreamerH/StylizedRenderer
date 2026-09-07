"""Create and export a plain gray platform for the viewer."""

import json
import struct
from pathlib import Path

import bpy


# Set this when the current .blend file is outside the StylizedRenderer repo.
PROJECT_ROOT = r"D:\StylizedRenderer"

PLATFORM_SIZE_METERS = 20.0
PLATFORM_COLOR = (0.55, 0.55, 0.55, 1.0)

OUTPUT_DIRECTORY = Path(
    PROJECT_ROOT,
    "assets",
    "models",
    "MMDmodel",
    "Plane",
)
GLB_PATH = OUTPUT_DIRECTORY / "grid_platform.glb"
SIDECAR_PATH = OUTPUT_DIRECTORY / "grid_platform.mtoon.json"


def enable_gltf_exporter() -> None:
    try:
        bpy.ops.preferences.addon_enable(module="io_scene_gltf2")
    except Exception:
        pass


def clear_scene() -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def create_material() -> bpy.types.Material:
    existing = bpy.data.materials.get("GridMaterial")
    if existing is not None and existing.users == 0:
        bpy.data.materials.remove(existing)

    material = bpy.data.materials.new("GridMaterial")
    material.use_nodes = True
    material.use_backface_culling = True

    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (480.0, 0.0)
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.location = (160.0, 0.0)
    shader.inputs["Base Color"].default_value = PLATFORM_COLOR
    shader.inputs["Metallic"].default_value = 0.0
    shader.inputs["Roughness"].default_value = 0.92

    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return material


def create_platform(material: bpy.types.Material) -> bpy.types.Object:
    bpy.ops.mesh.primitive_plane_add(
        size=PLATFORM_SIZE_METERS,
        location=(0.0, 0.0, 0.0),
    )
    plane = bpy.context.active_object
    plane.name = "GridPlatform"
    plane.data.name = "GridPlatformMesh"
    plane.data.materials.append(material)

    if any(polygon.normal.z <= 0.0 for polygon in plane.data.polygons):
        raise RuntimeError("Grid platform normals must face Blender +Z.")

    return plane


def export_glb(plane: bpy.types.Object) -> None:
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    plane.select_set(True)
    bpy.context.view_layer.objects.active = plane

    options = {
        "filepath": str(GLB_PATH),
        "check_existing": False,
        "export_format": "GLB",
        "use_selection": True,
        "export_cameras": False,
        "export_lights": False,
        "export_extras": False,
        "export_yup": True,
        "export_apply": True,
        "export_animations": False,
        "export_materials": "EXPORT",
        "export_texcoords": True,
        "export_normals": True,
        "export_tangents": False,
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


def exported_material_name() -> str:
    data = GLB_PATH.read_bytes()
    if len(data) < 20:
        raise RuntimeError("Exported GLB is smaller than its header.")

    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or total_length != len(data):
        raise RuntimeError("Exported file is not a valid GLB 2.0 container.")

    json_length, json_type = struct.unpack_from("<II", data, 12)
    if json_type != 0x4E4F534A or 20 + json_length > len(data):
        raise RuntimeError("Exported GLB has no valid JSON chunk.")

    document = json.loads(
        data[20:20 + json_length]
        .rstrip(b" \t\r\n\0")
        .decode("utf-8")
    )
    materials = document.get("materials", [])
    if len(materials) != 1:
        raise RuntimeError(
            f"Expected one exported material, found {len(materials)}."
        )

    name = materials[0].get("name")
    if not isinstance(name, str) or not name:
        raise RuntimeError("Exported GLB material has no name.")
    return name


def write_sidecar(material_name: str) -> None:
    payload = {
        "version": 3,
        "materials": [
            {
                "name": material_name,
                "baseColorFactor": list(PLATFORM_COLOR),
                "shadeColor": [0.28, 0.28, 0.28],
                "shadingShift": 0.0,
                "shadingShiftTextureScale": 1.0,
                "shadingToony": 0.65,
                "normalScale": 1.0,
                "surfaceOffset": 0.0,
                "shadowNormalInfluence": 0.0,
                "castShadow": False,
                "receiveShadow": True,
                "shadowCutoffEnabled": False,
                "shadowCutoff": 0.5,
                "giEqualization": 0.65,
                "screenOutline": {
                    "enabled": False,
                    "depthEnabled": False,
                    "normalEnabled": False,
                    "detectSelfDepth": False,
                    "detectSelfNormal": False,
                },
                "outline": {
                    "enabled": False,
                    "widthMode": "world",
                    "width": 1.0,
                    "color": [0.0, 0.0, 0.0],
                    "lightingMix": 0.0,
                },
                "textures": {
                    "baseColor": "",
                    "shade": "",
                    "toonRamp": "",
                    "normal": "",
                    "shadingShift": "",
                    "matcap": "",
                    "rimMask": "",
                    "emission": "",
                    "outlineWidthMask": "",
                    "occlusion": "",
                    "specular": "",
                },
            }
        ],
    }
    SIDECAR_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def main() -> None:
    enable_gltf_exporter()
    clear_scene()
    material = create_material()
    plane = create_platform(material)
    export_glb(plane)
    material_name = exported_material_name()
    write_sidecar(material_name)
    print(f"Grid platform GLB: {GLB_PATH}", flush=True)
    print(f"Exported material: {material_name}", flush=True)
    print(f"MToon sidecar: {SIDECAR_PATH}", flush=True)


if __name__ == "__main__":
    main()
