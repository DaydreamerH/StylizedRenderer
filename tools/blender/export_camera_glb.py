"""Export a Blender camera as a standalone JSON camera track.

Run this script from Blender after opening the source .blend file. Only JSON
is written. Character meshes, armatures, materials, lights, and
physics objects are not included.

The camera is sampled from Blender's evaluated dependency graph so constraints
and drivers are represented by the final camera transform. The sampled track
is reduced with position, rotation, and FOV error limits before being written
to JSON. The JSON keeps the original frame range, coordinate system, camera
projection, and reduced keyframes.
"""

import json
import math
from pathlib import Path

import bpy
from bpy_extras.io_utils import axis_conversion
from mathutils import Quaternion, Vector


# Edit these values in Blender's Text Editor when running the script directly.
# An empty output path writes <blend-name>.camera.json beside the .blend file.
OUTPUT_PATH = ""
CAMERA_NAME = ""
CAMERA_FRAME_START: int | None = None
CAMERA_FRAME_END: int | None = None
CAMERA_CUT_FRAMES: tuple[int, ...] = ()

# The camera and character may come from different Blender scenes with
# opposite forward conventions. Rotate the complete camera track in glTF
# world space so position and orientation remain consistent. Use 0.0 when
# both assets share the same scene convention.
TARGET_SCENE_YAW_DEGREES = 180.0

# The exporter evaluates every integer frame, then keeps only keys required to
# stay within these errors.  Set a threshold to 0.0 to keep every sample for
# that component.
POSITION_ERROR_METERS = 1.0e-4
ROTATION_ERROR_DEGREES = 0.05
FOV_ERROR_DEGREES = 0.01

BLENDER_TO_GLTF = axis_conversion(
    from_forward="-Y",
    from_up="Z",
    to_forward="-Z",
    to_up="Y",
).to_4x4()

TARGET_SCENE_ALIGNMENT = Quaternion(
    (0.0, 1.0, 0.0),
    math.radians(TARGET_SCENE_YAW_DEGREES),
).to_matrix().to_4x4()

def log(message: str) -> None:
    print(f"[stylized-camera-export] {message}", flush=True)


def find_camera(scene: bpy.types.Scene) -> bpy.types.Object:
    if CAMERA_NAME:
        camera = bpy.data.objects.get(CAMERA_NAME)
        if camera is None or camera.type != "CAMERA":
            raise RuntimeError(
                f"Camera object not found: {CAMERA_NAME}"
            )
        return camera

    if scene.camera is not None and scene.camera.type == "CAMERA":
        return scene.camera

    cameras = [
        obj for obj in scene.objects
        if obj.type == "CAMERA"
    ]
    if not cameras:
        raise RuntimeError("No camera exists in the current scene.")
    if len(cameras) > 1:
        log(
            "active scene camera is not set; using the first camera: "
            f"{cameras[0].name}"
        )
    return cameras[0]


def action_key_frames(id_data: object) -> list[float]:
    animation_data = getattr(id_data, "animation_data", None)
    action = (
        animation_data.action
        if animation_data is not None
        else None
    )
    if action is None:
        return []

    frames: list[float] = []
    for fcurve in action.fcurves:
        frames.extend(
            float(point.co[0])
            for point in fcurve.keyframe_points
        )
    return frames


def action_cut_frames(id_data: object) -> set[int]:
    animation_data = getattr(id_data, "animation_data", None)
    action = (
        animation_data.action
        if animation_data is not None
        else None
    )
    if action is None:
        return set()

    frames: set[int] = set()
    for fcurve in action.fcurves:
        points = fcurve.keyframe_points
        for index in range(len(points) - 1):
            if points[index].interpolation != "CONSTANT":
                continue
            frame = float(points[index + 1].co[0])
            if frame.is_integer():
                frames.add(int(frame))
    return frames


def resolve_frame_range(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
) -> tuple[int, int]:
    source_range = source_action_range(camera)

    if CAMERA_FRAME_START is not None:
        frame_start = int(CAMERA_FRAME_START)
    elif source_range is not None:
        frame_start = math.floor(source_range[0])
    else:
        frame_start = int(scene.frame_start)

    if CAMERA_FRAME_END is not None:
        frame_end = int(CAMERA_FRAME_END)
    elif source_range is not None:
        frame_end = math.ceil(source_range[1])
    else:
        frame_end = int(scene.frame_end)

    if frame_start > frame_end:
        raise RuntimeError(
            f"Invalid camera frame range: {frame_start}..{frame_end}"
        )

    # If the camera has an action, its key range is the camera's real range.
    # Constraint-only cameras have no action range and use the scene range;
    # explicit CAMERA_FRAME_* values always take precedence.
    return frame_start, frame_end


def source_action_range(
    camera: bpy.types.Object,
) -> tuple[float, float] | None:
    frames = action_key_frames(camera)
    frames.extend(action_key_frames(camera.data))
    if not frames:
        return None
    return min(frames), max(frames)


def evaluate_camera_samples(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    frame_start: int,
    frame_end: int,
) -> list[dict]:
    if camera.data.type != "PERSP":
        raise RuntimeError(
            f"Only perspective cameras are supported: {camera.name}"
        )

    original_frame = scene.frame_current
    depsgraph = bpy.context.evaluated_depsgraph_get()
    fps = float(scene.render.fps) / float(scene.render.fps_base)
    samples: list[dict] = []
    previous_rotation: Quaternion | None = None

    try:
        for frame in range(frame_start, frame_end + 1):
            scene.frame_set(frame)
            bpy.context.view_layer.update()

            evaluated_camera = camera.evaluated_get(depsgraph)
            # Camera local axes already match glTF: -Z is forward and +Y is
            # up. Only convert the world-space basis; conjugating the matrix
            # would incorrectly convert the camera's local axes a second time.
            matrix = (
                TARGET_SCENE_ALIGNMENT @
                BLENDER_TO_GLTF @
                evaluated_camera.matrix_world
            )
            translation = matrix.translation.copy()
            rotation = matrix.to_quaternion().normalized()

            # q and -q represent the same orientation.  Keeping adjacent
            # quaternions on the same hemisphere makes reduction stable.
            if (
                previous_rotation is not None
                and previous_rotation.dot(rotation) < 0.0
            ):
                rotation = Quaternion(
                    (-rotation.w, -rotation.x, -rotation.y, -rotation.z)
                )
            previous_rotation = rotation.copy()

            samples.append(
                {
                    "frame": frame,
                    "timeSeconds": round(frame / fps, 9),
                    "timeFromStartSeconds": round(
                        (frame - frame_start) / fps,
                        9,
                    ),
                    "translation": translation,
                    "rotation": rotation,
                    "verticalFovDegrees": math.degrees(
                        evaluated_camera.data.angle_y
                    ),
                }
            )
    finally:
        scene.frame_set(original_frame)
        bpy.context.view_layer.update()

    return samples


def protected_frame_indices(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    samples: list[dict],
) -> set[int]:
    if not samples:
        return set()

    frames = set(action_key_frames(camera))
    frames.update(action_key_frames(camera.data))
    first_frame = int(samples[0]["frame"])
    last_frame = int(samples[-1]["frame"])
    cut_frames = camera_cut_frames(
        scene,
        camera,
        first_frame,
        last_frame,
    )
    frames.update(float(frame) for frame in cut_frames)
    frames.update(
        float(frame - 1)
        for frame in cut_frames
        if frame > first_frame
    )
    frames.add(float(first_frame))
    frames.add(float(last_frame))

    protected: set[int] = set()
    for frame in frames:
        index = min(
            range(len(samples)),
            key=lambda candidate: abs(
                float(samples[candidate]["frame"]) - frame
            ),
        )
        protected.add(index)
    return protected


def slerp(a: Quaternion, b: Quaternion, factor: float) -> Quaternion:
    result = a.copy()
    result.slerp(b, factor)
    return result


def normalized_error(value: float, threshold: float) -> float:
    if threshold <= 0.0:
        return math.inf if value > 1.0e-12 else 0.0
    return value / threshold


def simplify_samples(
    samples: list[dict],
    protected: set[int],
) -> list[dict]:
    if len(samples) <= 2:
        return samples

    keep: set[int] = set(protected)

    def split_segment(start: int, end: int) -> None:
        if end <= start + 1:
            return

        protected_between = sorted(
            index
            for index in protected
            if start < index < end
        )
        if protected_between:
            split = protected_between[0]
            keep.add(split)
            split_segment(start, split)
            split_segment(split, end)
            return

        first = samples[start]
        last = samples[end]
        frame_span = float(last["frame"] - first["frame"])
        if frame_span <= 0.0:
            return

        max_score = 0.0
        max_index: int | None = None

        for index in range(start + 1, end):
            sample = samples[index]
            factor = (
                float(sample["frame"] - first["frame"])
                / frame_span
            )

            expected_translation = first["translation"].lerp(
                last["translation"],
                factor,
            )
            expected_rotation = slerp(
                first["rotation"],
                last["rotation"],
                factor,
            )
            expected_fov = (
                first["verticalFovDegrees"]
                + (
                    last["verticalFovDegrees"]
                    - first["verticalFovDegrees"]
                )
                * factor
            )

            position_error = (
                sample["translation"] - expected_translation
            ).length
            rotation_error = math.degrees(
                sample["rotation"].rotation_difference(
                    expected_rotation
                ).angle
            )
            fov_error = abs(
                float(sample["verticalFovDegrees"])
                - expected_fov
            )
            score = max(
                normalized_error(
                    position_error,
                    POSITION_ERROR_METERS,
                ),
                normalized_error(
                    rotation_error,
                    ROTATION_ERROR_DEGREES,
                ),
                normalized_error(
                    fov_error,
                    FOV_ERROR_DEGREES,
                ),
            )

            if score > max_score:
                max_score = score
                max_index = index

        if max_index is not None and max_score > 1.0:
            keep.add(max_index)
            split_segment(start, max_index)
            split_segment(max_index, end)

    split_segment(0, len(samples) - 1)
    return [
        samples[index]
        for index in sorted(keep)
    ]


def vector_to_list(value: Vector) -> list[float]:
    return [round(float(component), 9) for component in value]


def quaternion_to_list(value: Quaternion) -> list[float]:
    return [
        round(float(value.w), 9),
        round(float(value.x), 9),
        round(float(value.y), 9),
        round(float(value.z), 9),
    ]


def camera_cut_markers(
    scene: bpy.types.Scene,
    frame_start: int,
    frame_end: int,
) -> list[dict]:
    result: list[dict] = []
    for marker in sorted(
        scene.timeline_markers,
        key=lambda item: item.frame,
    ):
        if not frame_start <= marker.frame <= frame_end:
            continue
        name = marker.name.strip()
        lowered = name.casefold()
        if getattr(marker, "camera", None) is not None or any(
            token in lowered
            for token in ("camera", "shot", "cut")
        ):
            result.append(
                {
                    "frame": int(marker.frame),
                    "name": name,
                }
            )
    return result


def camera_cut_frames(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    frame_start: int,
    frame_end: int,
) -> set[int]:
    frames = {
        int(marker["frame"])
        for marker in camera_cut_markers(
            scene,
            frame_start,
            frame_end,
        )
    }
    frames.update(
        int(frame)
        for frame in CAMERA_CUT_FRAMES
        if frame_start <= int(frame) <= frame_end
    )
    frames.update(
        frame
        for frame in action_cut_frames(camera) |
            action_cut_frames(camera.data)
        if frame_start <= frame <= frame_end
    )
    return frames


def build_sidecar(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    reduced_samples: list[dict],
    frame_start: int,
    frame_end: int,
    source_range: tuple[float, float] | None,
) -> dict:
    fps = float(scene.render.fps) / float(scene.render.fps_base)
    cut_frames = camera_cut_frames(
        scene,
        camera,
        frame_start,
        frame_end,
    )
    keyframes = []
    for index, sample in enumerate(reduced_samples):
        next_frame = (
            int(reduced_samples[index + 1]["frame"])
            if index + 1 < len(reduced_samples)
            else None
        )
        keyframes.append(
            {
                "frame": int(sample["frame"]),
                "timeSeconds": sample["timeSeconds"],
                "timeFromStartSeconds": sample[
                    "timeFromStartSeconds"
                ],
                "translation": vector_to_list(
                    sample["translation"]
                ),
                "rotation": quaternion_to_list(
                    sample["rotation"]
                ),
                "verticalFovDegrees": round(
                    float(sample["verticalFovDegrees"]),
                    6,
                ),
                "interpolation": (
                    "STEP"
                    if next_frame in cut_frames
                    else "LINEAR"
                ),
            }
        )

    payload = {
        "version": 2,
        "camera": camera.name,
        "coordinateSystem": "gltf-y-up",
        "rotationOrder": "wxyz",
        "fps": fps,
        "frameStart": frame_start,
        "frameEnd": frame_end,
        "timeStart": round(frame_start / fps, 9),
        "timeEnd": round(frame_end / fps, 9),
        "durationSeconds": round(
            (frame_end - frame_start) / fps,
            9,
        ),
        "nearPlane": round(float(camera.data.clip_start), 6),
        "farPlane": round(float(camera.data.clip_end), 6),
        "sceneFrameStart": int(scene.frame_start),
        "sceneFrameEnd": int(scene.frame_end),
        "sourceActionFrameStart": (
            round(source_range[0], 6)
            if source_range is not None
            else None
        ),
        "sourceActionFrameEnd": (
            round(source_range[1], 6)
            if source_range is not None
            else None
        ),
        "keyframes": keyframes,
        "cameraCuts": camera_cut_markers(
            scene,
            frame_start,
            frame_end,
        ),
        "cameraCutFrames": sorted(cut_frames),
    }
    return payload


def resolve_output_path() -> Path:
    if OUTPUT_PATH:
        destination = Path(bpy.path.abspath(OUTPUT_PATH))
    else:
        if not bpy.data.filepath:
            raise RuntimeError(
                "Save the current .blend file or configure OUTPUT_PATH."
            )
        destination = Path(bpy.data.filepath)

    if destination.suffix.casefold() != ".json":
        destination = destination.with_suffix(".json")

    return destination


def main() -> None:
    scene = bpy.context.scene
    source_camera = find_camera(scene)
    frame_start, frame_end = resolve_frame_range(
        scene,
        source_camera,
    )
    source_range = source_action_range(source_camera)

    log(f"source camera: {source_camera.name}")
    log(f"frame range: {frame_start}..{frame_end}")
    if source_range is not None:
        log(
            "source action range: "
            f"{source_range[0]:.3f}..{source_range[1]:.3f}"
        )

    samples = evaluate_camera_samples(
        scene,
        source_camera,
        frame_start,
        frame_end,
    )
    protected = protected_frame_indices(
        scene,
        source_camera,
        samples,
    )
    reduced_samples = simplify_samples(samples, protected)
    log(
        f"evaluated samples: {len(samples)}; "
        f"exported keyframes: {len(reduced_samples)}"
    )

    destination = resolve_output_path()
    destination.parent.mkdir(parents=True, exist_ok=True)

    payload = build_sidecar(
        scene,
        source_camera,
        reduced_samples,
        frame_start,
        frame_end,
        source_range,
    )
    destination.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )

    size_kib = destination.stat().st_size / 1024.0
    log(f"camera JSON: {destination} ({size_kib:.1f} KiB)")
    log("DONE")


if __name__ == "__main__":
    main()
