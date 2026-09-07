import os
import re

import bpy

PMX_PATH = r"D:\GoogleDownload\Tda式初音ミクV4X_Ver1.00\TdaMikuV4X\TdaMikuV4X.pmx"
VMD_PATH = r"D:\GoogleDownload\Mikudance\dance\gokuraku_upper_body.vmd"
OUT_PATH = r"D:\GoogleDownload\Mikudance\tda_gokuraku_b36.glb"
BLEND_PATH = r"D:\GoogleDownload\Mikudance\tda_gokuraku_b36.blend"

SCALE = 1.0
MARGIN = 10
FPS = 30


def log(*args):
    print("[mmd2glb36]", *args, flush=True)


def call_op(op, kwargs):
    remaining = dict(kwargs)
    while remaining:
        try:
            return op(**remaining)
        except TypeError as exc:
            match = re.search(r"keyword\s+\"([^\"]+)\"", str(exc))
            if not match:
                raise
            key = match.group(1)
            if key not in remaining:
                raise
            log("drop unsupported param:", key)
            del remaining[key]
    return op()


def enable_mmd_tools():
    import addon_utils

    try:
        if addon_utils.enable(
                "mmd_tools", default_set=True, persistent=True):
            return
    except Exception as exc:
        log("enable failed:", exc)
    raise RuntimeError("MMD Tools not available in Blender 3.6 addons")


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.outliner.orphans_purge(do_recursive=True)


def select_model_objects():
    armatures = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not armatures:
        raise RuntimeError("no armature found after model import")
    armature = max(
        armatures,
        key=lambda o: len(o.data.bones) if o.data else 0,
    )
    meshes = [
        o
        for o in bpy.data.objects
        if o.type == "MESH"
        and not o.hide_get()
        and any(
            m.type == "ARMATURE" and m.object == armature
            for m in o.modifiers
        )
    ]
    if not meshes:
        raise RuntimeError("no skinned mesh found")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    return armature, meshes


def convert_materials_for_gltf():
    converted = 0
    for material in bpy.data.materials:
        if not material.node_tree:
            continue
        mmd = getattr(material, "mmd_material", None)
        image = None
        if mmd is not None and mmd.diffuse_texture:
            image = mmd.diffuse_texture
        if image is None:
            image = next(
                (n.image for n in material.node_tree.nodes
                 if n.type == "TEX_IMAGE" and n.image),
                None,
            )
        if image is None:
            continue
        nodes = material.node_tree.nodes
        nodes.clear()
        output = nodes.new(type="ShaderNodeOutputMaterial")
        bsdf = nodes.new(type="ShaderNodeBsdfPrincipled")
        texture = nodes.new(type="ShaderNodeTexImage")
        texture.image = image
        material.node_tree.links.new(
            texture.outputs["Color"], bsdf.inputs["Base Color"])
        material.node_tree.links.new(
            bsdf.outputs["BSDF"], output.inputs["Surface"])
        converted += 1
    log("converted materials:", converted)


def verify_physics(armature, scene):
    def sample(frame):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        arm_eval = armature.evaluated_get(
            bpy.context.evaluated_depsgraph_get())
        out = {}
        for name in ("スカート_0_0", "スカート_4_15", "髪１.R"):
            if name in arm_eval.pose.bones:
                out[name] = arm_eval.pose.bones[name].matrix.copy()
        return out

    a = sample(100)
    b = sample(3000)
    for name, ma in a.items():
        mb = b.get(name)
        if mb is None:
            continue
        qa = ma.to_quaternion()
        qb = mb.to_quaternion()
        diff = (qa.inverted() @ qb).angle
        log(f"verify {name}: rot diff={diff:.3f} rad")
        return diff


def main():
    for path in (PMX_PATH, VMD_PATH):
        if not os.path.isfile(path):
            raise RuntimeError("file not found: " + path)
    os.makedirs(os.path.dirname(os.path.abspath(OUT_PATH)), exist_ok=True)

    clear_scene()
    enable_mmd_tools()
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1.0

    log("import model:", PMX_PATH)
    call_op(bpy.ops.mmd_tools.import_model, {
        "filepath": PMX_PATH,
        "scale": SCALE,
        "clean_model": False,
        "remove_doubles": False,
        "fix_IK_links": False,
        "apply_bone_fixed_axis": False,
        "rename_bones": False,
        "use_underscore": False,
        "use_mipmap": True,
        "sph_blend_factor": 1.0,
        "spa_blend_factor": 1.0,
    })

    armature, meshes = select_model_objects()
    log("imported", len(meshes), "mesh(es), armature:", armature.name)

    log("import motion:", VMD_PATH)
    call_op(bpy.ops.mmd_tools.import_vmd, {
        "filepath": VMD_PATH,
        "scale": SCALE,
        "margin": MARGIN,
        "camera_animation": False,
        "rename_bones": False,
        "use_underscore": False,
        "update_scene_settings": True,
    })

    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = max(1, scene.frame_end)

    action = armature.animation_data.action if armature.animation_data else None
    if action:
        frame_max = 1
        for fcurve in action.fcurves:
            for key in fcurve.keyframe_points:
                frame_max = max(frame_max, int(key.co[0]))
        scene.frame_end = max(scene.frame_end, frame_max)
    log("frame range:", scene.frame_start, "..", scene.frame_end)

    log("assemble rigid body world")
    bpy.context.view_layer.objects.active = armature
    scene.frame_set(1)
    bpy.ops.mmd_tools.assemble_all()

    point_cache = scene.rigidbody_world.point_cache
    point_cache.frame_start = 1
    point_cache.frame_end = scene.frame_end

    log("bake rigid body cache: 1 ..", scene.frame_end)
    scene.frame_set(1)
    bpy.ops.mmd_tools.ptcache_rigid_body_bake()

    physics_bones = [
        pose_bone
        for pose_bone in armature.pose.bones
        if "mmd_tools_rigid_track" in pose_bone.constraints
    ]
    log("physics bones:", len(physics_bones))

    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="DESELECT")
    for pose_bone in physics_bones:
        pose_bone.bone.select = True
    log("bake physics into bone keyframes (visual)")
    call_op(bpy.ops.nla.bake, {
        "frame_start": 1,
        "frame_end": scene.frame_end,
        "step": 1,
        "only_selected": True,
        "visual_keying": True,
        "clear_constraints": False,
        "clear_parents": False,
        "use_current_action": True,
        "bake_types": {"POSE"},
    })
    bpy.ops.object.mode_set(mode="OBJECT")

    log("disassemble physics")
    bpy.context.view_layer.objects.active = armature
    bpy.ops.mmd_tools.disassemble_all()

    verify_physics(armature, scene)

    armature, meshes = select_model_objects()
    convert_materials_for_gltf()
    log("export glb:", OUT_PATH)
    call_op(bpy.ops.export_scene.gltf, {
        "filepath": OUT_PATH,
        "export_format": "GLB",
        "use_selection": True,
        "export_yup": True,
        "export_apply": True,
        "export_animations": True,
        "export_animation_mode": "ACTIONS",
        "export_force_sampling": True,
        "export_frame_step": 1,
        "export_def_bones": False,
        "export_morph": True,
        "export_skins": True,
        "export_current_frame": False,
    })

    if BLEND_PATH:
        bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
        log("saved blend:", BLEND_PATH)

    log("DONE ->", OUT_PATH)


if __name__ == "__main__":
    main()
