"""Generate a repeatable UV-space face-SDF test texture in Blender.

This script does not require Photoshop, painting, or a pre-existing SDF map.
It creates a grayscale *threshold field* for the Face SDF algorithm planned by
StylizedRenderer.  The renderer mirrors the U coordinate according to the
head-local light direction and compares this value against a light-angle
threshold.  Therefore this is intentionally not a Euclidean 3D signed
distance field.

Use it to validate, in order:
    1. face-material selection and UVs;
    2. linear (non-sRGB) texture loading;
    3. light-side UV mirroring;
    4. face-SDF offset and softness controls;
    5. head animation and shadow composition.

The map is independent of mesh geometry: only the face material's UV layout
matters.  Its face-shaped curvature is controlled by the settings below, so a
technical artist can later replace it with a painted map without changing the
renderer integration.

Run from Blender's Text Editor with a face mesh selected.  Save the .blend
first, then run the script.  It saves ``face_sdf_procedural.png`` beside the
.blend by default and adds an unconnected Image Texture node to the selected
face material for inspection.
"""

from __future__ import annotations

from array import array
from pathlib import Path

import bpy
import bmesh


# ---------------------------------------------------------------------------
# User settings
# ---------------------------------------------------------------------------

# Leave empty to use the selected object's active material.  Set this to the
# exact face material name when the active material slot is not the face.
TARGET_MATERIAL_NAME = ""

# The output is written beside the current .blend file.  Set an absolute path
# here instead if the generated texture should live elsewhere.
OUTPUT_PATH = ""
OUTPUT_FILENAME = "face_sdf_procedural.png"

IMAGE_NAME = "FaceSdf_Procedural"
RESOLUTION = 1024

# A positive U gradient makes the right side lighter in the generated image.
# If the first renderer test lights the wrong side, do not repaint the image:
# first verify the shader's mirror condition, then optionally negate this.
HORIZONTAL_RANGE = 0.44

# These terms bow the light/shadow boundary so it is less like a flat vertical
# split.  Start small.  They are deliberately easy to tune without painting.
CHEEK_CURVATURE = 0.11
FOREHEAD_BIAS = -0.025
CHIN_BIAS = 0.045


def log(message: str) -> None:
    print(f"[procedural-face-sdf] {message}", flush=True)


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


def resolve_output_path() -> Path:
    if OUTPUT_PATH:
        return Path(bpy.path.abspath(OUTPUT_PATH)).resolve()

    if not bpy.data.filepath:
        raise RuntimeError(
            "Save the .blend file first, or set OUTPUT_PATH to an absolute "
            "path in this script.")

    return Path(bpy.data.filepath).parent / OUTPUT_FILENAME


def find_target_material() -> tuple[bpy.types.Object, bpy.types.Material]:
    active_object = bpy.context.active_object
    if active_object is None or active_object.type != "MESH":
        raise RuntimeError(
            "Select the mesh that owns the face material, then run again.")

    if TARGET_MATERIAL_NAME:
        material = bpy.data.materials.get(TARGET_MATERIAL_NAME)
        if material is None:
            raise RuntimeError(
                f"Material '{TARGET_MATERIAL_NAME}' was not found.")
    else:
        material = active_object.active_material
        if material is None:
            raise RuntimeError(
                f"Mesh '{active_object.name}' has no active material. Select "
                "the face material slot first, or set TARGET_MATERIAL_NAME.")

    if material.name not in {slot.name for slot in active_object.data.materials
                             if slot is not None}:
        raise RuntimeError(
            f"Material '{material.name}' is not assigned to selected mesh "
            f"'{active_object.name}'.")

    return active_object, material


def material_uv_bounds(
    mesh_object: bpy.types.Object,
    material: bpy.types.Material) -> tuple[float, float, float, float]:
    """Find the UV rectangle used by this material on the selected mesh."""
    mesh = mesh_object.data

    material_indices = {
        index for index, slot in enumerate(mesh.materials)
        if slot == material
    }

    minimum_u = float("inf")
    minimum_v = float("inf")
    maximum_u = float("-inf")
    maximum_v = float("-inf")

    if mesh_object.mode == "EDIT":
        # Mesh.uv_layers data is intentionally unavailable while Blender owns
        # the editable BMesh.  Read its active UV layer directly instead.
        edit_mesh = bmesh.from_edit_mesh(mesh)
        uv_layer = edit_mesh.loops.layers.uv.active

        if uv_layer is None:
            raise RuntimeError(
                f"Mesh '{mesh_object.name}' has no active UV map.")

        for face in edit_mesh.faces:
            if face.material_index not in material_indices:
                continue

            for loop in face.loops:
                uv = loop[uv_layer].uv
                minimum_u = min(minimum_u, uv.x)
                minimum_v = min(minimum_v, uv.y)
                maximum_u = max(maximum_u, uv.x)
                maximum_v = max(maximum_v, uv.y)
    else:
        uv_layer = mesh.uv_layers.active

        if uv_layer is None:
            raise RuntimeError(
                f"Mesh '{mesh_object.name}' has no active UV map.")

        for polygon in mesh.polygons:
            if polygon.material_index not in material_indices:
                continue

            for loop_index in polygon.loop_indices:
                uv = uv_layer.data[loop_index].uv
                minimum_u = min(minimum_u, uv.x)
                minimum_v = min(minimum_v, uv.y)
                maximum_u = max(maximum_u, uv.x)
                maximum_v = max(maximum_v, uv.y)

    if (minimum_u == float("inf") or
            maximum_u - minimum_u < 1.0e-6 or
            maximum_v - minimum_v < 1.0e-6):
        raise RuntimeError(
            f"Material '{material.name}' has no usable UV area on "
            f"'{mesh_object.name}'.")

    return minimum_u, minimum_v, maximum_u, maximum_v


def threshold_value(
    u: float,
    v: float,
    uv_bounds: tuple[float, float, float, float]) -> float:
    """Return a 0..1 face-light threshold from one UV coordinate."""
    minimum_u, minimum_v, maximum_u, maximum_v = uv_bounds
    center_u = 0.5 * (minimum_u + maximum_u)
    center_v = 0.5 * (minimum_v + maximum_v)
    half_width = 0.5 * (maximum_u - minimum_u)
    half_height = 0.5 * (maximum_v - minimum_v)

    x = (u - center_u) / max(half_width, 1.0e-6)
    y = (v - center_v) / max(half_height, 1.0e-6)

    # ``y * y`` creates symmetric cheek curvature.  Interpolate the forehead
    # and chin terms continuously: a conditional switch at y == 0 produces a
    # visible horizontal line in the generated texture.
    forehead_weight = clamp01(0.5 + 0.5 * y)
    vertical_bias = (
        CHEEK_CURVATURE * y * y +
        CHIN_BIAS * (1.0 - forehead_weight) +
        FOREHEAD_BIAS * forehead_weight
    )

    return clamp01(0.5 + HORIZONTAL_RANGE * x + vertical_bias)


def create_image(
    uv_bounds: tuple[float, float, float, float]) -> bpy.types.Image:
    existing = bpy.data.images.get(IMAGE_NAME)
    if existing is not None:
        bpy.data.images.remove(existing)

    image = bpy.data.images.new(
        IMAGE_NAME,
        width=RESOLUTION,
        height=RESOLUTION,
        alpha=False,
        float_buffer=False,
    )

    # Face SDF is numeric comparison data, never display color data.
    try:
        image.colorspace_settings.name = "Non-Color"
    except (TypeError, ValueError):
        log("warning: Blender has no 'Non-Color' image setting; use linear "
            "import in the renderer.")

    pixels = array("f", [0.0]) * (RESOLUTION * RESOLUTION * 4)
    index = 0

    for y in range(RESOLUTION):
        v = (y + 0.5) / RESOLUTION

        for x in range(RESOLUTION):
            u = (x + 0.5) / RESOLUTION
            value = threshold_value(u, v, uv_bounds)

            pixels[index] = value
            pixels[index + 1] = value
            pixels[index + 2] = value
            pixels[index + 3] = 1.0
            index += 4

    image.pixels.foreach_set(pixels)
    image.update()
    return image


def save_image(image: bpy.types.Image, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.filepath_raw = str(output_path)
    image.file_format = "PNG"
    image.save()


def add_inspection_node(
    material: bpy.types.Material,
    image: bpy.types.Image) -> None:
    material.use_nodes = True
    nodes = material.node_tree.nodes

    node_name = "StylizedRenderer Face SDF"
    node = nodes.get(node_name)

    if node is None:
        node = nodes.new("ShaderNodeTexImage")
        node.name = node_name
        node.label = "Face SDF (procedural test)"
        node.location = (-520.0, -420.0)

    node.image = image
    node.interpolation = "Linear"
    node.projection = "FLAT"


def main() -> None:
    if RESOLUTION < 16:
        raise RuntimeError("RESOLUTION must be at least 16.")

    output_path = resolve_output_path()
    mesh_object, material = find_target_material()
    uv_bounds = material_uv_bounds(mesh_object, material)
    image = create_image(uv_bounds)
    save_image(image, output_path)
    add_inspection_node(material, image)

    log(f"saved linear face-SDF test texture: {output_path}")
    log(f"attached inspection node to material: {material.name}")
    log("face material UV bounds: "
        f"U={uv_bounds[0]:.3f}..{uv_bounds[2]:.3f}, "
        f"V={uv_bounds[1]:.3f}..{uv_bounds[3]:.3f}")
    log("next: inspect its UV island, then bind this file through the "
        "renderer's future Character Profile faceSdfTexture field.")


main()
