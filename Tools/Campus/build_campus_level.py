"""UE 5.8 Python commandlet: build /Game/Jokgu/Maps/L_CampusEnclosed from the CampusEnclosed GLB.

- New blank level with sky lighting, BP_JGCourt (placeholder visuals off) and BP_JGGameMode override.
- Interchange scene import of Exports/DJG_CampusEnclosed.glb, rotated so the Blender court length (Y)
  runs along the game court length (X). Background meshes have no collision; the court owns the floor.
- Spectators (05/07/08/09/10), referee (11) and cheerleader (12) as looping SkeletalMeshActors.
Run with the editor closed. Re-running rebuilds the level from scratch.
Usually run through rebuild_campus.py, which applies cleanup_campus_level.py afterwards.
"""
import json
import math
import traceback
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = PROJECT / "Art/Schoolyard/CampusEnclosed/Exports/DJG_CampusEnclosed.glb"
REPORT = PROJECT / "Tools/Campus/Reports/build_report.json"
LEVEL = "/Game/Jokgu/Maps/L_CampusEnclosed"
ASSETS = "/Game/Jokgu/Environment/CampusEnclosed"
RIG = "/Game/Jokgu/Characters/Rigged"
EAL = unreal.EditorAssetLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Blender metres (x, y), look, looping animation, start offset in seconds
EXTRAS = [
    ("Spectator_05", (-11.6, -3.0), "05_BlondKicker", "Clap", 0.0),
    ("Spectator_07", (-11.6, -1.2), "07_Headband", "Confidence", 0.4),
    ("Spectator_08", (-11.6, 3.0), "08_BackwardCap", "Taunt", 0.8),
    ("Spectator_09", (15.5, -4.0), "09_GreenGlasses", "Clap", 0.3),
    ("Spectator_10", (15.5, 4.5), "10_RedBob", "Victory", 0.6),
    ("Cheerleader_12", (-10.4, 1.0), "12_Cheerleader", "Victory", 0.0),
    ("Referee_11", (8.2, -1.2), "11_Referee", "Idle", 0.0),
]
EXPOSURE_BIAS = 0.0  # absolute EV: one stop darker than the project default of +1
RESULT = {"engine": unreal.SystemLibrary.get_engine_version(), "success": False, "warnings": []}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load(path):
    asset = EAL.load_asset(path)
    require(asset, "Missing asset " + path)
    return asset


def current_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def new_level():
    if EAL.does_asset_exist(LEVEL):
        require(EAL.delete_asset(LEVEL), "Could not delete old " + LEVEL)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    require(world, "Could not create a blank map")
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL), "Could not save " + LEVEL)
    world = current_world()
    require(world and world.get_path_name().startswith(LEVEL), "Editor world is not " + LEVEL)
    return world


def spawn(cls, location=(0, 0, 0), yaw=0.0, pitch=0.0, label=None):
    actor = ACTORS.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(0.0, pitch, yaw))
    require(actor, "Could not spawn " + str(cls))
    if label:
        actor.set_actor_label(label)
    return actor


def setup_gameplay(world):
    sun = spawn(unreal.DirectionalLight, (0, 0, 800), yaw=-35.0, pitch=-50.0, label="Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)
    sky = spawn(unreal.SkyLight, (0, 0, 600), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    spawn(unreal.ExponentialHeightFog, (0, 0, -100), label="HeightFog")
    spawn(unreal.VolumetricCloud, label="VolumetricCloud")
    spawn(unreal.PlayerStart, (0, 0, 92), label="PlayerStart")

    # project default is fixed exposure with +1 bias, which washes out the bright campus art
    volume = spawn(unreal.PostProcessVolume, label="PostProcess")
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", EXPOSURE_BIAS)
    volume.set_editor_property("settings", settings)
    RESULT["exposure_bias"] = EXPOSURE_BIAS

    court = spawn(unreal.EditorAssetLibrary.load_blueprint_class("/Game/Jokgu/Gameplay/BP_JGCourt"), label="Court")
    # set_editor_property notifies PostEditChange, which reruns OnConstruction
    court.set_editor_property("show_placeholder_visuals", False)
    visible = [c.get_name() for c in court.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
    RESULT["court_visible_meshes"] = visible
    require(not visible, "Court placeholder meshes are still visible")

    game_mode = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Jokgu/Core/BP_JGGameMode")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)


def import_background(world):
    before = set(a.get_path_name() for a in ACTORS.get_all_level_actors())
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(str(SOURCE))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    # import_level left unset: Interchange uses the current editor level
    require(manager.import_scene(ASSETS, source, params), "Interchange scene import failed")
    manager.wait_until_all_tasks_done(False)
    imported = [a for a in ACTORS.get_all_level_actors() if a.get_path_name() not in before]
    require(imported, "Scene import created no actors")
    # the commandlet does not save what Interchange creates, so the level would point at transient meshes
    require(unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True), "Could not save imported assets")
    assets = EAL.list_assets(ASSETS, True, False)
    require(assets, "No assets saved under " + ASSETS)
    RESULT["saved_assets"] = len(assets)
    missing = [a.get_actor_label() for a in imported if isinstance(a, unreal.StaticMeshActor)
               and not a.static_mesh_component.static_mesh]
    require(not missing, "Actors without meshes: " + ", ".join(missing[:5]))
    return imported


def find(actors, token, skip=()):
    for actor in actors:
        label = actor.get_actor_label()
        if token in label and not any(s in label for s in skip):
            return actor
    return None


def align_background(actors):
    """Rotates the import about the origin until the net runs along Y and the court length along X."""
    roots = [a for a in actors if a.get_attach_parent_actor() is None]
    net = find(actors, "NetMesh")
    require(net, "NetMesh actor not found in import")
    _, extent = net.get_actor_bounds(False)
    if extent.x > extent.y:
        for actor in roots:  # +90 degree yaw about the world origin
            v = actor.get_actor_location()
            actor.set_actor_location(unreal.Vector(-v.y, v.x, v.z), False, True)
            actor.add_actor_world_rotation(unreal.Rotator(0.0, 0.0, 90.0), False, True)
    _, net_extent = net.get_actor_bounds(False)
    lines = find(actors, "CourtLines")
    _, line_extent = lines.get_actor_bounds(False) if lines else (None, unreal.Vector())
    RESULT["net_extent_cm"] = [net_extent.x, net_extent.y, net_extent.z]
    RESULT["court_lines_extent_cm"] = [line_extent.x, line_extent.y, line_extent.z]
    require(net_extent.y > net_extent.x, "Net still runs along X after alignment")


def fit_court_art(actors):
    """Scales the art court lines and net about the court centre to the judgement court (outer line edges)."""
    balance = unreal.JGBalanceData.get_balance_data()
    half = {"x": balance.get_editor_property("court_length") * 0.5, "y": balance.get_editor_property("court_width") * 0.5}
    targets = {"CourtLines": (half["x"], half["y"]),
               "NetMesh": (None, half["y"] + 50.0),  # same span as the net collision
               "NetPosts": (None, half["y"] + 50.0)}
    fitted = {}
    for token, (want_x, want_y) in targets.items():
        actor = find(actors, token)
        require(actor, token + " actor not found in import")
        origin, extent = actor.get_actor_bounds(False)
        location = actor.get_actor_location()
        require(abs(location.x) < 5.0 and abs(location.y) < 5.0, token + " pivot is not at the court centre")
        world_scale = (want_x / extent.x if want_x else 1.0, want_y / extent.y)
        forward = actor.get_actor_forward_vector()
        local = world_scale if abs(forward.x) > 0.5 else (world_scale[1], world_scale[0])
        scale = actor.get_actor_scale3d()
        actor.set_actor_scale3d(unreal.Vector(scale.x * local[0], scale.y * local[1], scale.z))
        _, after = actor.get_actor_bounds(False)
        fitted[token] = {"before_cm": [round(extent.x * 2, 1), round(extent.y * 2, 1)],
                         "after_cm": [round(after.x * 2, 1), round(after.y * 2, 1), round(origin.z + extent.z, 1)]}
        require(abs(after.y - want_y) < 1.0, token + " did not reach the target width")
    RESULT["court_art_fit"] = fitted
    ground = find(actors, "SM_DJG_Ground")
    if ground:
        origin, extent = ground.get_actor_bounds(False)
        RESULT["art_ground_top_cm"] = round(origin.z + extent.z, 2)


def strip_collision(actors):
    count = 0
    for actor in actors:
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
            count += 1
    RESULT["background_components"] = count


def blender_to_world(actors):
    """Maps Blender metres to world cm using two landmarks, so no axis convention is assumed."""
    chair = find(actors, "RefereeChair")
    bleacher = find(actors, "Bleacher", skip=("Rail",))
    require(chair and bleacher, "Landmark actors (RefereeChair, Bleacher) not found")
    c = chair.get_actor_location()
    # chair sits at Blender (9.5, 0); the import is turned in 90 degree steps, so snap to an axis
    ex = unreal.Vector(float(round(c.x / 950.0)), float(round(c.y / 950.0)), 0.0)
    require(abs(ex.x) + abs(ex.y) == 1.0, "Could not resolve the Blender X axis from the chair")
    ey = unreal.Vector(-ex.y, ex.x, 0.0)
    b = bleacher.get_actor_location()
    if (b.x * ey.x + b.y * ey.y) < 0.0:  # every bleacher sits at Blender y = +23
        ey = unreal.Vector(-ey.x, -ey.y, 0.0)
    RESULT["blender_x_axis"] = [ex.x, ex.y]
    RESULT["blender_y_axis"] = [ey.x, ey.y]
    return lambda bx, by: unreal.Vector(100.0 * (bx * ex.x + by * ey.x), 100.0 * (bx * ex.y + by * ey.y), 0.0)


def place_extras(to_world):
    placed = []
    for label, (bx, by), look, animation, offset in EXTRAS:
        location = to_world(bx, by)
        yaw = math.degrees(math.atan2(-location.y, -location.x))  # face the net centre, mesh front is +X
        actor = spawn(unreal.SkeletalMeshActor, (location.x, location.y, 0.0), yaw=yaw, label=label)
        component = actor.skeletal_mesh_component
        component.set_skeletal_mesh_asset(load(RIG + "/Meshes/SK_DJG_" + look))
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        play = unreal.SingleAnimationPlayData()
        play.set_editor_property("anim_to_play", load(RIG + "/Animations/A_DJG_" + animation))
        play.set_editor_property("saved_looping", True)
        play.set_editor_property("saved_playing", True)
        play.set_editor_property("saved_position", offset)
        play.set_editor_property("saved_play_rate", 1.0)
        component.set_editor_property("animation_data", play)
        actor.set_folder_path("Crowd")
        placed.append({"label": label, "look": look, "animation": animation,
                       "location_cm": [round(location.x), round(location.y)], "yaw": round(yaw, 1)})
    RESULT["extras"] = placed


def main():
    world = new_level()
    setup_gameplay(world)
    imported = import_background(world)
    RESULT["imported_actors"] = len(imported)
    for actor in imported:
        if actor.get_attach_parent_actor() is None:
            actor.set_folder_path("Background")
    align_background(imported)
    fit_court_art(imported)
    strip_collision(imported)
    place_extras(blender_to_world(imported))
    require(current_world().get_path_name().startswith(LEVEL), "Editor world changed during the build")
    require(unreal.EditorLoadingAndSavingUtils.save_map(current_world(), LEVEL), "Could not save the level")
    RESULT["success"] = True


def run():
    try:
        main()
    except Exception:
        RESULT["error"] = traceback.format_exc()
    REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("CAMPUS_LEVEL " + ("OK" if RESULT["success"] else "FAILED"))
    return RESULT["success"]


if __name__ == "__main__":
    run()
