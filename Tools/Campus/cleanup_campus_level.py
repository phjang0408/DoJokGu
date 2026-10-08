"""UE 5.8 Python commandlet step: applies campus_edits.json to /Game/Jokgu/Maps/L_CampusEnclosed.

Runs right after build_campus_level.py (see rebuild_campus.py), so every edit survives a rebuild.
Actors are found by label (GLB node name with "." -> "_"). Edits:
- delete: labels to remove, grouped by reason
- transform: per label, optional location [x, y, z] cm, yaw deg, uniform scale
- add: new StaticMeshActors {label, mesh, location, yaw, scale}
- collision: mesh base names (label == base or base_<n>) for walkable floors and blockers inside the wall. Pawns collide; the
  JGBall channel and the camera channel are ignored so the ball and match camera are unaffected.
Writes Tools/Campus/Reports/cleanup_report.json.
"""
import json
import traceback
from pathlib import Path
import unreal

TOOLS = Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"
EDITS = TOOLS / "campus_edits.json"
REPORT = TOOLS / "Reports/cleanup_report.json"
LEVEL = "/Game/Jokgu/Maps/L_CampusEnclosed"
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EAL = unreal.EditorAssetLibrary
BALL_CHANNEL = unreal.CollisionChannel.ECC_GAME_TRACE_CHANNEL1  # "JGBall" in DefaultEngine.ini
RESULT = {"success": False, "warnings": []}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def current_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def mesh_triangles(component):
    mesh = component.static_mesh
    try:
        return mesh.get_num_triangles(0) if mesh else 0
    except Exception:  # render data may be missing under -nullrhi; the Blender report has the counts too
        return 0


def background_stats():
    actors = [a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)]
    return {"actors": len(actors), "triangles": sum(mesh_triangles(a.static_mesh_component) for a in actors)}


def by_label():
    return {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}


def apply_deletes(edits, actors):
    removed = {}
    for reason, labels in edits.get("delete", {}).items():
        count = 0
        for name in labels:
            actor = actors.pop(name, None)
            if actor is None:
                RESULT["warnings"].append("delete: no actor " + name)
                continue
            require(ACTORS.destroy_actor(actor), "Could not delete " + name)
            count += 1
        removed[reason] = count
    RESULT["deleted"] = removed


def apply_transforms(edits, actors):
    done = 0
    for edit in edits.get("transform", []):
        actor = actors.get(edit["label"])
        if actor is None:
            RESULT["warnings"].append("transform: no actor " + edit["label"])
            continue
        if "location" in edit:
            actor.set_actor_location(unreal.Vector(*edit["location"]), False, True)
        if "yaw" in edit:
            rotation = actor.get_actor_rotation()
            actor.set_actor_rotation(unreal.Rotator(rotation.roll, rotation.pitch, edit["yaw"]), True)
        if "scale" in edit:
            actor.set_actor_scale3d(unreal.Vector(edit["scale"], edit["scale"], edit["scale"]))
        done += 1
    RESULT["transformed"] = done


def apply_adds(edits, actors):
    done = 0
    for edit in edits.get("add", []):
        mesh = EAL.load_asset(edit["mesh"])
        require(mesh, "Missing mesh " + edit["mesh"])
        location = unreal.Vector(*edit["location"])
        rotation = unreal.Rotator(0.0, 0.0, edit.get("yaw", 0.0))
        actor = actors.get(edit["label"])
        if actor:  # cleanup re-run on an already cleaned map: move the earlier copy instead of duplicating
            actor.set_actor_location_and_rotation(location, rotation, False, True)
        else:
            actor = ACTORS.spawn_actor_from_object(mesh, location, rotation)
        require(actor, "Could not spawn " + edit["label"])
        actor.set_actor_label(edit["label"])
        scale = edit.get("scale", 1.0)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_folder_path("Background/Added")
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actors[edit["label"]] = actor
        done += 1
    RESULT["added"] = done


def inside(actor, site):
    location = actor.get_actor_location()
    return site[0] <= location.x <= site[1] and site[2] <= location.y <= site[3]


def make_collidable(actor, walkable, meshes):
    component = actor.static_mesh_component
    mesh = component.static_mesh
    if mesh and mesh.get_path_name() not in meshes:
        # imported meshes carry no simple shapes: collide against the render triangles
        body = mesh.get_editor_property("body_setup")
        require(body, "No body setup on " + mesh.get_path_name())
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        meshes[mesh.get_path_name()] = mesh
    component.set_collision_profile_name("BlockAll")
    component.set_collision_response_to_channel(BALL_CHANNEL, unreal.CollisionResponseType.ECR_IGNORE)
    component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)
    component.set_editor_property("can_character_step_up_on",
                                  unreal.CanBeCharacterBase.ECB_YES if walkable else unreal.CanBeCharacterBase.ECB_NO)


def apply_collision(edits, actors):
    rules = edits.get("collision", {})
    site = rules.get("site_cm")
    require(site and len(site) == 4, "collision.site_cm must be [x min, x max, y min, y max]")
    meshes, counts = {}, {"walk": 0, "block": 0}
    for name, actor in actors.items():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        for kind in ("walk", "block"):
            if any(name == base or name.startswith(base + "_") for base in rules.get(kind, [])):
                # floors may extend past the wall (terrain, plazas); blockers only matter inside it
                if kind == "walk" or inside(actor, site):
                    make_collidable(actor, kind == "walk", meshes)
                    counts[kind] += 1
                break
    for mesh in meshes.values():
        require(EAL.save_loaded_asset(mesh, False), "Could not save " + mesh.get_path_name())
    RESULT["collision"] = dict(counts, meshes=len(meshes))


def main():
    edits = json.loads(EDITS.read_text(encoding="utf-8"))
    require(unreal.EditorLoadingAndSavingUtils.load_map(LEVEL), "Could not load " + LEVEL)
    require(current_world().get_path_name().startswith(LEVEL), "Editor world is not " + LEVEL)
    RESULT["before"] = background_stats()
    actors = by_label()
    apply_deletes(edits, actors)
    apply_transforms(edits, actors)
    apply_adds(edits, actors)
    apply_collision(edits, actors)
    RESULT["after"] = background_stats()
    require(current_world().get_path_name().startswith(LEVEL), "Editor world changed during cleanup")
    require(unreal.EditorLoadingAndSavingUtils.save_map(current_world(), LEVEL), "Could not save the level")
    RESULT["success"] = True


def run():
    try:
        main()
    except Exception:
        RESULT["error"] = traceback.format_exc()
    REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("CAMPUS_CLEANUP " + ("OK" if RESULT["success"] else "FAILED"))
    return RESULT["success"]


if __name__ == "__main__":
    run()
