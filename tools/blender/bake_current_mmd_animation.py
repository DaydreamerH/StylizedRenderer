"""Bake the currently loaded MMD pose animation for glTF export.

Usage in Blender:
    1. Import the PMX model and its dance/lip VMD files with MMD Tools.
    2. Save the .blend file once.
    3. Open this file in the Scripting workspace and choose Run Script.

The script builds MMD rigid-body physics, bakes its point cache, then bakes the
evaluated pose transforms and driver-controlled Shape Keys at every frame. It
removes pose-bone constraints from the baked rig. A source backup is created
before the scene changes and the result is saved as ``*_baked.blend``.
"""

from datetime import datetime
from pathlib import Path

import bpy
from mathutils import Vector


BAKED_ACTION_NAME = "MMD_Baked"
FRAME_STEP = 1
PHYSICS_WARMUP_FRAMES = 60
ENABLE_MMD_PHYSICS = True
CLEAR_CONSTRAINTS = False
CLEAN_CURVES = False
FORCE_REBAKE = False
MORPH_EPSILON = 1.0e-4


def log(message: str) -> None:
    print(f"[stylized-bake] {message}", flush=True)


def find_armature() -> bpy.types.Object:
    active = bpy.context.view_layer.objects.active
    if active is not None and active.type == "ARMATURE":
        return active

    armatures = [
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "ARMATURE"
    ]
    if not armatures:
        raise RuntimeError("No armature exists in the current scene.")

    # MMD scenes normally contain one main armature. If there is more than one,
    # the armature with the largest bone count is the least surprising default.
    return max(armatures, key=lambda obj: len(obj.data.bones))


def find_mmd_root(armature: bpy.types.Object) -> bpy.types.Object:
    obj = armature

    while obj is not None:
        if getattr(obj, "mmd_type", "NONE") == "ROOT":
            return obj
        obj = obj.parent

    roots = [
        obj
        for obj in bpy.context.scene.objects
        if getattr(obj, "mmd_type", "NONE") == "ROOT"
    ]
    if len(roots) == 1:
        return roots[0]

    raise RuntimeError(
        "Could not uniquely find the MMD root for the armature."
    )


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


def unique_shape_keys(
    meshes: list[bpy.types.Object],
) -> list[bpy.types.Key]:
    result: list[bpy.types.Key] = []
    seen: set[int] = set()

    for mesh in meshes:
        shape_keys = mesh.data.shape_keys
        if shape_keys is None or len(shape_keys.key_blocks) <= 1:
            continue

        pointer = shape_keys.as_pointer()
        if pointer in seen:
            continue

        seen.add(pointer)
        result.append(shape_keys)

    return result


def sample_shape_key_animation(
    meshes: list[bpy.types.Object],
    frame_start: int,
    frame_end: int,
) -> list[tuple[bpy.types.Key, list[list[float]]]]:
    """Capture evaluated MMD morph weights before physics changes the rig."""
    scene = bpy.context.scene
    previous_frame = scene.frame_current
    shape_key_sets = unique_shape_keys(meshes)
    if not shape_key_sets:
        return []

    samples: dict[int, list[list[float]]] = {
        shape_keys.as_pointer(): []
        for shape_keys in shape_key_sets
    }

    log("capturing source MMD morphs before physics bake...")
    try:
        for frame in range(frame_start, frame_end + 1, FRAME_STEP):
            scene.frame_set(frame)
            bpy.context.view_layer.update()

            for shape_keys in shape_key_sets:
                samples[shape_keys.as_pointer()].append([
                    float(key_block.value)
                    for key_block in list(shape_keys.key_blocks)[1:]
                ])
    finally:
        scene.frame_set(previous_frame)
        bpy.context.view_layer.update()

    return [
        (shape_keys, samples[shape_keys.as_pointer()])
        for shape_keys in shape_key_sets
    ]


def bake_shape_key_animation(
    captured_samples: list[
        tuple[bpy.types.Key, list[list[float]]]
    ],
    frame_start: int,
    frame_end: int,
) -> tuple[int, int]:
    """Write captured MMD morph weights as explicit Shape Key actions."""
    if not captured_samples:
        return 0, 0

    animated_set_count = 0
    animated_target_count = 0

    for shape_keys, sampled_frames in captured_samples:
        key_blocks = list(shape_keys.key_blocks)[1:]
        animated_indices = [
            index
            for index in range(len(key_blocks))
            if (
                max(values[index] for values in sampled_frames)
                - min(values[index] for values in sampled_frames)
            ) > MORPH_EPSILON
        ]

        if not animated_indices:
            continue

        # The captured values contain the final result of the original MMD
        # Root morph controls. Remove only the animated target drivers in the
        # baked copy so the explicit action becomes the source of truth.
        for index in animated_indices:
            data_path = key_blocks[index].path_from_id("value")
            shape_keys.driver_remove(data_path)

        animation_data = shape_keys.animation_data_create()
        baked_action = bpy.data.actions.new(
            name=f"{shape_keys.name}_Morph_Baked"
        )
        animation_data.action = baked_action

        frames = list(range(frame_start, frame_end + 1, FRAME_STEP))
        for index in animated_indices:
            key_block = key_blocks[index]
            previous_value: float | None = None

            for sample_index, frame in enumerate(frames):
                value = sampled_frames[sample_index][index]
                is_endpoint = sample_index in (0, len(frames) - 1)
                value_changed = (
                    previous_value is None
                    or abs(value - previous_value) > MORPH_EPSILON
                )
                if not is_endpoint and not value_changed:
                    continue

                key_block.value = value
                key_block.keyframe_insert(
                    data_path="value",
                    frame=frame,
                    group="MMD Morphs",
                )
                previous_value = value

        # VMD morph weights interpolate continuously between keyed frames.
        if hasattr(baked_action, "fcurves"):
            for fcurve in baked_action.fcurves:
                for keyframe in fcurve.keyframe_points:
                    keyframe.interpolation = "LINEAR"

        animated_set_count += 1
        animated_target_count += len(animated_indices)

    return animated_set_count, animated_target_count


def save_source_backup(source_path: Path) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = source_path.with_name(
        f"{source_path.stem}_source_backup_{timestamp}.blend"
    )

    result = bpy.ops.wm.save_as_mainfile(
        filepath=str(backup_path),
        copy=True,
        check_existing=False,
    )
    if "FINISHED" not in result:
        raise RuntimeError("Blender could not save the source backup.")

    return backup_path


def select_armature(armature: bpy.types.Object) -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    bpy.ops.object.select_all(action="DESELECT")
    armature.hide_set(False)
    armature.hide_viewport = False
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="DESELECT")


def is_physics_bone(pose_bone: bpy.types.PoseBone) -> bool:
    # MMD Tools creates this exact constraint only for bones whose final
    # transform is driven by a dynamic rigid body. A broad target-based test
    # also catches IK and helper bones on some PMX rigs, overwriting their VMD
    # animation during the selective physics bake.
    return pose_bone.constraints.get("mmd_tools_rigid_track") is not None


def select_physics_bones(armature: bpy.types.Object) -> list[str]:
    select_armature(armature)
    selected: list[str] = []

    for pose_bone in armature.pose.bones:
        if not is_physics_bone(pose_bone):
            continue

        # Blender 5.0 moved pose-mode selection from Bone.select to
        # PoseBone.select. Keep the fallback for Blender 4.x.
        if hasattr(pose_bone, "select"):
            pose_bone.select = True
        else:
            pose_bone.bone.select = True
        selected.append(pose_bone.name)

    if not selected:
        raise RuntimeError(
            "MMD Tools did not create any rigid-body-driven pose bones."
        )

    return selected


def activate_object(obj: bpy.types.Object) -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def point_cache_override(
    scene: bpy.types.Scene,
    point_cache: bpy.types.PointCache,
):
    return bpy.context.temp_override(
        scene=scene,
        point_cache=point_cache,
    )


def free_rigid_body_cache(scene: bpy.types.Scene) -> None:
    rigid_body_world = scene.rigidbody_world
    if rigid_body_world is None:
        return

    point_cache = rigid_body_world.point_cache
    if not point_cache.is_baked:
        return

    log("deleting the existing rigid-body cache...")
    with point_cache_override(scene, point_cache):
        result = bpy.ops.ptcache.free_bake()
    if "FINISHED" not in result:
        raise RuntimeError(
            f"Could not delete the old physics cache: {result}"
        )


def rebuild_mmd_model(armature: bpy.types.Object) -> None:
    root = find_mmd_root(armature)

    if root.mmd_root.is_built:
        # Reassembling an already working MMD model recreates its driver
        # graph. VMD facial morph animation lives on that graph, so doing so
        # here silently resets every evaluated Shape Key weight to zero.
        # Keep the setup that the user has already verified in Blender and
        # only rebuild the rigid-body point cache below.
        log("reusing the existing MMD morph and physics setup...")
        return

    log("assembling MMD bones, morphs, SDEF and rigid bodies...")
    activate_object(armature)
    result = bpy.ops.mmd_tools.assemble_all()
    if "FINISHED" not in result:
        raise RuntimeError(f"MMD setup assembly failed: {result}")


def bake_mmd_physics(
    armature: bpy.types.Object,
    frame_start: int,
    frame_end: int,
) -> None:
    if not hasattr(bpy.ops, "mmd_tools"):
        raise RuntimeError(
            "MMD Tools is not enabled in this Blender installation."
        )

    scene = bpy.context.scene
    simulation_start = frame_start - PHYSICS_WARMUP_FRAMES
    original_scene_start = scene.frame_start

    # MMD joints and rigid bodies are authored relative to the reference pose.
    # Building them on an arbitrary animation frame initializes Blender's
    # rigid bodies in a different world-space pose and makes dynamic parts
    # appear pinned, stretched or exploded.
    # Evaluate the animation before its first key. Blender's default constant
    # F-Curve extrapolation holds the first authored pose while the rigid-body
    # system settles. These warm-up frames are cached but are not included in
    # the baked pose action or exported animation.
    scene.frame_start = simulation_start
    scene.frame_set(simulation_start)
    bpy.context.view_layer.update()

    free_rigid_body_cache(scene)
    rebuild_mmd_model(armature)

    scene.frame_set(simulation_start)
    bpy.context.view_layer.update()

    result = bpy.ops.mmd_tools.rigid_body_world_update()
    if "FINISHED" not in result:
        raise RuntimeError(f"Rigid Body World update failed: {result}")

    rigid_body_world = scene.rigidbody_world
    if rigid_body_world is None:
        raise RuntimeError(
            "MMD Tools did not create a Rigid Body World."
        )

    rigid_body_world.enabled = True
    rigid_body_world.substeps_per_frame = 6
    rigid_body_world.solver_iterations = 10

    point_cache = rigid_body_world.point_cache
    point_cache.frame_start = simulation_start
    point_cache.frame_end = frame_end
    scene.frame_set(simulation_start)

    log(
        "baking MMD rigid-body cache with "
        f"{PHYSICS_WARMUP_FRAMES} warm-up frames; "
        "Blender may appear unresponsive..."
    )
    try:
        with point_cache_override(scene, point_cache):
            result = bpy.ops.ptcache.bake(bake=True)
    finally:
        scene.frame_start = original_scene_start

    if "FINISHED" not in result:
        raise RuntimeError(f"Rigid-body cache bake failed: {result}")

    if not point_cache.is_baked:
        raise RuntimeError(
            "Rigid-body cache was not baked. Check Blender's console."
        )

    log(
        "MMD rigid-body cache baked: "
        f"{point_cache.frame_start}..{point_cache.frame_end}; "
        f"exported animation starts at {frame_start}"
    )


def count_pose_constraints(armature: bpy.types.Object) -> int:
    return sum(
        len(pose_bone.constraints)
        for pose_bone in armature.pose.bones
    )


def evaluated_mesh_bounds(
    meshes: list[bpy.types.Object],
) -> dict[str, tuple[Vector, Vector]]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    bounds: dict[str, tuple[Vector, Vector]] = {}

    for mesh in meshes:
        evaluated = mesh.evaluated_get(depsgraph)
        corners = [
            evaluated.matrix_world @ Vector(corner)
            for corner in evaluated.bound_box
        ]
        minimum = Vector((
            min(corner.x for corner in corners),
            min(corner.y for corner in corners),
            min(corner.z for corner in corners),
        ))
        maximum = Vector((
            max(corner.x for corner in corners),
            max(corner.y for corner in corners),
            max(corner.z for corner in corners),
        ))
        bounds[mesh.name] = (minimum, maximum)

    return bounds


def validate_baked_mesh_bounds(
    before: dict[str, tuple[Vector, Vector]],
    after: dict[str, tuple[Vector, Vector]],
) -> None:
    invalid: list[str] = []

    for mesh_name, (before_minimum, before_maximum) in before.items():
        after_bounds = after.get(mesh_name)
        if after_bounds is None:
            invalid.append(f"{mesh_name} (missing after bake)")
            continue

        after_minimum, after_maximum = after_bounds
        before_size = (before_maximum - before_minimum).length
        after_size = (after_maximum - after_minimum).length
        before_center = (before_minimum + before_maximum) * 0.5
        after_center = (after_minimum + after_maximum) * 0.5

        size_limit = max(before_size * 3.0, 0.001)
        center_limit = max(before_size * 2.0, 0.001)

        if (
            after_size > size_limit
            or (after_center - before_center).length > center_limit
        ):
            invalid.append(
                f"{mesh_name} "
                f"(size {before_size:.3f} -> {after_size:.3f})"
            )

    if invalid:
        names = ", ".join(invalid[:8])
        raise RuntimeError(
            "The baked pose differs drastically from the source pose. "
            "Animation or physics was probably applied twice. "
            f"Affected meshes: {names}. Reload the original .blend file."
        )


def prepare_bake_action(
    armature: bpy.types.Object,
) -> bpy.types.Action:
    animation_data = armature.animation_data_create()
    source_action = animation_data.action

    if source_action is None:
        raise RuntimeError(
            "The armature has no active VMD action. Select the imported "
            "motion in Blender before running the script."
        )

    # Preserve the source motion exactly. Only the rigid-body-driven bones
    # will be overwritten with evaluated physics keys below.
    baked_action = source_action.copy()
    baked_action.name = BAKED_ACTION_NAME
    animation_data.action = baked_action
    return baked_action


def make_action_interpolation_linear(
    action: bpy.types.Action,
) -> int:
    """Prevent Blender from exporting VMD curves as CUBICSPLINE tracks.

    Imported VMD actions are already densely keyed. Changing interpolation
    does not evaluate the rig again or introduce constraint noise; it only
    makes glTF store each existing value as an ordinary LINEAR key instead
    of serializing Bezier tangents as quaternion spline data.
    """
    if not hasattr(action, "fcurves"):
        raise RuntimeError(
            "The baked action does not expose legacy F-curves."
        )

    key_count = 0
    for fcurve in action.fcurves:
        for keyframe in fcurve.keyframe_points:
            keyframe.interpolation = "LINEAR"
            key_count += 1

    return key_count


def detach_baked_physics(
    armature: bpy.types.Object,
    physics_bone_names: list[str],
) -> int:
    """Stop live rigid-body driving without dismantling the MMD rig.

    ``mmd_tools.disassemble_all()`` also removes the IK and additional-
    transform constraints used by ordinary MMD animation.  Only the bones
    carrying ``mmd_tools_rigid_track`` were baked above, so dismantling the
    complete rig leaves leg IK controls (and meshes driven by them) without
    their evaluated motion.

    The complete evaluated result of every rigid-body-driven bone is already
    stored as keyframes.  Its constraints must therefore be removed to avoid
    applying an additional transform twice.  Constraints on all other bones
    remain intact, including leg IK and its controller relationships.
    """
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    removed_count = 0
    for bone_name in physics_bone_names:
        pose_bone = armature.pose.bones.get(bone_name)
        if pose_bone is None:
            continue

        for constraint in list(pose_bone.constraints):
            pose_bone.constraints.remove(constraint)
            removed_count += 1

    rigid_body_world = bpy.context.scene.rigidbody_world
    if rigid_body_world is not None and hasattr(rigid_body_world, "enabled"):
        rigid_body_world.enabled = False

    return removed_count


def main(
    captured_morph_samples: list[
        tuple[bpy.types.Key, list[list[float]]]
    ] | None = None,
) -> None:
    source_filepath = bpy.data.filepath
    if not source_filepath:
        raise RuntimeError(
            "Save the current .blend file before running this script."
        )

    source_path = Path(source_filepath)
    scene = bpy.context.scene
    frame_start = int(scene.frame_start)
    frame_end = int(scene.frame_end)

    if frame_end <= frame_start:
        raise RuntimeError(
            f"Invalid scene frame range: {frame_start}..{frame_end}."
        )

    armature = find_armature()
    if armature.get("stylized_pose_baked", False) and not FORCE_REBAKE:
        raise RuntimeError(
            "This armature is already marked as baked. "
            "Reload the source backup or set FORCE_REBAKE = True."
        )

    meshes = find_bound_meshes(armature)
    previous_frame = scene.frame_current
    scene.frame_set(previous_frame)
    bpy.context.view_layer.update()
    mesh_bounds_before = evaluated_mesh_bounds(meshes)
    constraint_count = count_pose_constraints(armature)

    # MMD facial animation is evaluated through drivers owned by the MMD
    # root. Building or rebaking rigid-body physics can invalidate that
    # driver graph for some models, so preserve its evaluated result before
    # any scene mutation takes place.
    morph_samples = captured_morph_samples
    if morph_samples is None:
        morph_samples = sample_shape_key_animation(
            meshes,
            frame_start,
            frame_end,
        )

    log(f"armature: {armature.name}")
    log(f"bound meshes: {len(meshes)}")
    log(f"frame range: {frame_start}..{frame_end}")
    log(f"pose constraints before bake: {constraint_count}")

    backup_path = save_source_backup(source_path)
    log(f"source backup: {backup_path}")

    baked_action = prepare_bake_action(armature)

    if ENABLE_MMD_PHYSICS:
        bake_mmd_physics(
            armature,
            frame_start,
            frame_end,
        )

    morph_action_count, morph_target_count = bake_shape_key_animation(
        morph_samples,
        frame_start,
        frame_end,
    )
    log(
        "baked Shape Key actions: "
        f"{morph_action_count}, animated targets: {morph_target_count}"
    )

    physics_bones = select_physics_bones(armature)
    log(f"physics bones selected for pose bake: {len(physics_bones)}")
    scene.frame_set(frame_start)

    log("baking evaluated physics bones; Blender may appear unresponsive...")
    result = bpy.ops.nla.bake(
        frame_start=frame_start,
        frame_end=frame_end,
        step=FRAME_STEP,
        only_selected=True,
        visual_keying=True,
        clear_constraints=CLEAR_CONSTRAINTS,
        clear_parents=False,
        use_current_action=True,
        clean_curves=CLEAN_CURVES,
        bake_types={"POSE"},
        channel_types={"LOCATION", "ROTATION", "SCALE"},
    )
    if "FINISHED" not in result:
        raise RuntimeError(f"Pose bake did not finish: {result}")

    if armature.animation_data.action != baked_action:
        raise RuntimeError("The pose bake did not create an action.")

    baked_action.name = BAKED_ACTION_NAME
    linear_key_count = make_action_interpolation_linear(baked_action)
    log(f"linear pose keys prepared for glTF: {linear_key_count}")
    armature["stylized_pose_baked"] = True
    armature["stylized_pose_baked_frame_start"] = frame_start
    armature["stylized_pose_baked_frame_end"] = frame_end

    bpy.ops.object.mode_set(mode="OBJECT")
    removed_physics_constraints = detach_baked_physics(
        armature,
        physics_bones,
    )
    log(
        "detached constraints from baked physics bones: "
        f"{removed_physics_constraints}; preserved MMD IK on other bones"
    )
    scene.frame_set(previous_frame)
    bpy.context.view_layer.update()
    validate_baked_mesh_bounds(
        mesh_bounds_before,
        evaluated_mesh_bounds(meshes),
    )

    baked_path = source_path.with_name(
        f"{source_path.stem}_baked.blend"
    )
    save_result = bpy.ops.wm.save_as_mainfile(
        filepath=str(baked_path),
        check_existing=False,
    )
    if "FINISHED" not in save_result:
        raise RuntimeError("Blender could not save the baked file.")

    log(f"baked action: {baked_action.name}")
    log(f"pose constraints after bake: {count_pose_constraints(armature)}")
    log(
        "Shape Key actions baked: "
        f"{morph_action_count}, targets: {morph_target_count}"
    )
    log(f"DONE: {baked_path}")


if __name__ == "__main__":
    main()
