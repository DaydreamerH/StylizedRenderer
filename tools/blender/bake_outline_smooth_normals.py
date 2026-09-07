"""Bake a separate smooth normal field for inverted-hull outlines.

Run this script in Blender's Text Editor with the target mesh objects selected.
It leaves the mesh's ordinary normals untouched and writes a new Point-domain
Float Color Attribute named ``outline_normal`` instead.  The RGB channels hold
the encoded normal (normal * 0.5 + 0.5); alpha is a per-vertex outline-width
multiplier reserved for the renderer.

The calculation is deliberately independent of Blender's split/loop normals:
it averages area-weighted polygon normals for vertices that occupy the same
position.  Consequently UV seams and ordinary hard-edge vertex splits no
longer make the inverted-hull extrusion point in different directions.

Important limitations
---------------------
* Processing happens per mesh object.  Do not select body, hair, and clothing
  together expecting their normals to be blended across objects.
* ``PROCESS_SELECTED_FACES_ONLY`` makes a combined character mesh practical:
  select only the hair faces in Edit Mode, then run the script.  Only those
  vertices are rewritten.
* The result is intended for the *outline* vertex shader only.  It must not
  replace the normal used by the MToon lighting pass.
* Use the rest/default pose.  At runtime the attribute must be skinned by the
  same JOINTS/WEIGHTS as the original normal.
* Deliberately separate hard parts (metal plates, accessories, etc.) into
  separate objects, or set ``MERGE_BY_POSITION`` to False for that object.

The Blender glTF exporter must be configured to export all vertex color
attributes.  A renderer can read this attribute from an additional COLOR_n
stream and decode it in the outline pass.
"""

from __future__ import annotations

from collections import defaultdict
from math import floor

import bpy
import bmesh
from mathutils import Vector


# ---------------------------------------------------------------------------
# User settings
# ---------------------------------------------------------------------------

# The Color Attribute written to each processed mesh.  Its RGB channels are
# encoded outline normals and its A channel is the outline width multiplier.
ATTRIBUTE_NAME = "outline_normal"

# Treat vertices within this local-space distance as one logical vertex.  This
# bridges vertices split by a glTF import/export or UV seam.  Keep this tiny:
# a larger value can accidentally join nearby but separate surfaces.
WELD_EPSILON = 1.0e-5

# Use a positional weld before averaging.  Set False if a selected mesh
# intentionally contains distinct, coincident surfaces that must not share an
# outline direction.
MERGE_BY_POSITION = True

# The initial value stored in the alpha channel.  The renderer may multiply its
# normal outline width by this value; paint or edit the attribute afterwards to
# vary / suppress the outline locally.
OUTLINE_WIDTH_MULTIPLIER = 1.0

# False: bake every polygon of each selected mesh object.
# True: bake only polygons selected in Edit Mode, and rewrite only vertices
# belonging to those selected polygons.  Use this for a combined character
# mesh when you want to process hair without changing the other parts.
PROCESS_SELECTED_FACES_ONLY = False


def log(message: str) -> None:
    print(f"[outline-smooth-normal] {message}", flush=True)


def quantize_coordinate(value: float) -> int:
    """Round consistently for positive and negative local coordinates."""
    scaled = value / WELD_EPSILON
    return (
        floor(scaled + 0.5)
        if scaled >= 0.0
        else -floor(-scaled + 0.5)
    )


def vertex_key(co: Vector, vertex_index: int) -> tuple[int, int, int] | int:
    if not MERGE_BY_POSITION:
        return vertex_index

    return (
        quantize_coordinate(co.x),
        quantize_coordinate(co.y),
        quantize_coordinate(co.z),
    )


def find_or_create_color_attribute(mesh: bpy.types.Mesh):
    attribute = mesh.color_attributes.get(ATTRIBUTE_NAME)
    if attribute is None:
        return (
            mesh.color_attributes.new(
                name=ATTRIBUTE_NAME,
                type="FLOAT_COLOR",
                domain="POINT",
            ),
            True,
        )

    if attribute.domain != "POINT" or attribute.data_type != "FLOAT_COLOR":
        raise RuntimeError(
            f"{mesh.name}: existing '{ATTRIBUTE_NAME}' must be a "
            "Point-domain Float Color Attribute. Rename or remove it first "
            "if it is used for something else."
        )

    return attribute, False


def encode_normal(normal: Vector) -> tuple[float, float, float]:
    normal = normal.normalized()
    encoded = normal * 0.5 + Vector((0.5, 0.5, 0.5))
    return encoded.x, encoded.y, encoded.z


def bake_mesh(
    mesh_object: bpy.types.Object,
    selected_face_indices: set[int] | None,
) -> None:
    mesh = mesh_object.data
    if not mesh.vertices or not mesh.polygons:
        log(f"skip {mesh_object.name}: no vertices or polygons")
        return

    mesh.update()

    # Position-key -> sum(area-weighted face normal).  We intentionally use
    # polygon normals rather than mesh vertex normals: the latter may already
    # be split by UV seams, Auto Smooth, or imported hard edges.
    accumulated_normals: dict[tuple[int, int, int] | int, Vector] = defaultdict(
        lambda: Vector((0.0, 0.0, 0.0))
    )
    contributing_faces: dict[tuple[int, int, int] | int, int] = defaultdict(int)

    polygons = (
        (
            mesh.polygons[index]
            for index in selected_face_indices
        )
        if selected_face_indices is not None
        else mesh.polygons
    )
    target_vertex_indices: set[int] = set()
    for polygon in polygons:
        area = polygon.area
        if area <= 1.0e-12:
            continue

        weighted_normal = polygon.normal * area
        for vertex_index in polygon.vertices:
            target_vertex_indices.add(vertex_index)
            key = vertex_key(mesh.vertices[vertex_index].co, vertex_index)
            accumulated_normals[key] += weighted_normal
            contributing_faces[key] += 1

    attribute, was_created = find_or_create_color_attribute(mesh)
    if was_created:
        # A selected-face bake is often an incremental operation on one
        # combined character mesh.  Initialize every other vertex from its
        # original normal so the renderer never reads an invalid zero vector
        # from this newly created attribute.
        for vertex in mesh.vertices:
            encoded = encode_normal(vertex.normal)
            attribute.data[vertex.index].color = (
                encoded[0],
                encoded[1],
                encoded[2],
                OUTLINE_WIDTH_MULTIPLIER,
            )

    default_normal_count = 0
    for vertex_index in target_vertex_indices:
        vertex = mesh.vertices[vertex_index]
        key = vertex_key(vertex.co, vertex.index)
        outline_normal = accumulated_normals.get(
            key,
            Vector((0.0, 0.0, 0.0)),
        )
        if outline_normal.length_squared <= 1.0e-16:
            # Degenerate or isolated vertices should stay harmless.  Blender's
            # existing vertex normal is the least surprising fallback.
            outline_normal = vertex.normal.copy()
            default_normal_count += 1
        else:
            outline_normal.normalize()

        encoded = encode_normal(outline_normal)
        attribute.data[vertex.index].color = (
            encoded[0],
            encoded[1],
            encoded[2],
            OUTLINE_WIDTH_MULTIPLIER,
        )

    mesh.update()
    logical_vertex_count = len(accumulated_normals)
    log(
        f"baked {mesh_object.name}: {len(target_vertex_indices)} target "
        f"vertices out of {len(mesh.vertices)}, "
        f"{logical_vertex_count} outline-normal groups, "
        f"{default_normal_count} fallback vertices"
    )


def main() -> None:
    if WELD_EPSILON <= 0.0:
        raise ValueError("WELD_EPSILON must be positive.")
    if not 0.0 <= OUTLINE_WIDTH_MULTIPLIER <= 1.0:
        raise ValueError(
            "OUTLINE_WIDTH_MULTIPLIER must be in [0, 1] for a color attribute."
        )

    # Capture targets *before* leaving Edit Mode.  The Text Editor has its own
    # UI context, and some Blender versions clear selected_objects while the
    # object.mode_set operator changes mode from there.
    edit_mode_targets = [
        obj for obj in bpy.context.objects_in_mode if obj.type == "MESH"
    ]
    edit_face_selections: dict[int, set[int]] = {}
    if PROCESS_SELECTED_FACES_ONLY:
        for obj in edit_mode_targets:
            edit_mesh = bmesh.from_edit_mesh(obj.data)
            edit_face_selections[obj.as_pointer()] = {
                face.index for face in edit_mesh.faces if face.select
            }
    selected_targets = [
        obj for obj in bpy.context.selected_objects if obj.type == "MESH"
    ]
    targets = edit_mode_targets or selected_targets
    if not targets:
        raise RuntimeError(
            "Select a mesh in Object Mode, or enter Edit Mode on a mesh, "
            "then run this script again."
        )

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    log(
        f"processing {len(targets)} selected mesh object(s); "
        f"attribute={ATTRIBUTE_NAME}, mergeByPosition={MERGE_BY_POSITION}, "
        f"epsilon={WELD_EPSILON:g}"
    )
    for mesh_object in targets:
        selected_face_indices = None
        if PROCESS_SELECTED_FACES_ONLY:
            selected_face_indices = edit_face_selections.get(
                mesh_object.as_pointer(),
                {
                    polygon.index
                    for polygon in mesh_object.data.polygons
                    if polygon.select
                },
            )
            if not selected_face_indices:
                raise RuntimeError(
                    f"{mesh_object.name}: select the target faces in Edit "
                    "Mode before running with PROCESS_SELECTED_FACES_ONLY "
                    "enabled."
                )

        bake_mesh(mesh_object, selected_face_indices)

    log("done. Save the .blend, then export all vertex color attributes.")


main()
