"""UE 5.8 Python commandlet step: copies the cleaned L_CampusEnclosed into the two maps the game runs.

- L_Campus_Match: same actors, GameMode override BP_JGGameMode (listen-server match).
- L_Campus_Lobby: GameMode override BP_JGLobbyGameMode; court, crowd and match PlayerStart removed;
  a PlayerStart on the open ground between the main building terrace and the court, a Match zone beside
  the court, a Settings zone at the student hall, invisible walls on the school wall line that only stop
  pawns, a NavMeshBoundsVolume over the school and wandering AI students (AJGStudentCharacter).
  The NavMesh itself is built afterwards (see README: ResavePackages -BuildNavigationData).
Runs after cleanup_campus_level.py (see rebuild_campus.py). L_CampusEnclosed itself is not changed.
Design note: the spec's shared Level Instance (LI_Campus) is replaced for now by rebuilding both copies
from the same pipeline, so a background edit still reaches both maps.
Writes Tools/Campus/Reports/lobby_maps_report.json.
"""
import json
import math
import traceback
from pathlib import Path
import unreal

TOOLS = Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"
EDITS = TOOLS / "campus_edits.json"
REPORT = TOOLS / "Reports/lobby_maps_report.json"
SOURCE = "/Game/Jokgu/Maps/L_CampusEnclosed"
MATCH = "/Game/Jokgu/Maps/L_Campus_Match"
LOBBY = "/Game/Jokgu/Maps/L_Campus_Lobby"
EAL = unreal.EditorAssetLibrary
LEVELS = unreal.EditorLoadingAndSavingUtils
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
BALL_CHANNEL = unreal.CollisionChannel.ECC_JG_BALL

# spawn search: below the terrace (x 1635-3340; its stage and bleacher rails hid the court from x 3000).
# The north goal (x 1194-1373, y +-268) and festival booths (x 898-1200, |y| >= 560) have no collision, so
# the camera boom (about 520 cm behind) would sit behind the goal net: stand beside the goal instead.
SPAWN_SEARCH_X = range(850, 650, -100)
SPAWN_SEARCH_Y = (420, -420)
SPAWN_FLOORS = ("SM_ENC_ForecourtPlazas", "SM_DJG_Ground", "SM_ENC_SchoolPlot", "SM_ENC_Path",
                "SM_ENC_ConnectedTerrain")
SPAWN_YAW = 180.0                 # look back at the court
MATCH_ZONE_XY = (0.0, -700.0)     # court side line opposite the bleachers (bleachers at y +2300)
NAV_HEIGHT = (-200.0, 800.0)      # NavMesh bounds: ground to the terrace, stage and ramps
RIG = "/Game/Jokgu/Characters/Rigged"
STUDENT_LOOKS = ("05_BlondKicker", "07_Headband", "08_BackwardCap", "09_GreenGlasses", "10_RedBob", "07_Headband")
STUDENT_EMOTES = ("Clap", "Confidence", "Victory", "Taunt")
STUDENT_GRID_CM = 400             # candidate homes: flat walkable floor on this grid inside the wall
STUDENT_KEEP_CLEAR = ((-900, 900, -700, 700), 800)  # court rect [x0, x1, y0, y1] and distance from the spawn
WALL_HEIGHT = 600.0
WALL_THICKNESS = 20.0
RESULT = {"success": False, "warnings": []}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def current_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def open_map(path):
    require(LEVELS.load_map(path), "Could not load " + path)
    require(current_world().get_path_name().startswith(path), "Editor world is not " + path)
    return current_world()


def copy_map(target):
    # a fresh copy each run, so the copies never drift from the cleaned source
    if EAL.does_asset_exist(target):
        require(EAL.delete_asset(target), "Could not delete the old " + target)
    require(EAL.duplicate_asset(SOURCE, target), "Could not copy " + SOURCE + " to " + target)


def set_game_mode(world, blueprint_path):
    game_mode = EAL.load_blueprint_class(blueprint_path)
    require(game_mode, "Missing " + blueprint_path)
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)


def save(world, path):
    require(current_world().get_path_name().startswith(path), "Editor world changed before saving " + path)
    require(LEVELS.save_map(world, path), "Could not save " + path)


def ground_z(world, x, y):
    """Top of the walkable floor at (x, y), from a visibility trace down through the campus."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 3000.0), unreal.Vector(x, y, -1000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    # depending on the binding the call returns the HitResult, (bool, HitResult) or None.
    # HitResult fields are not Python properties here; to_tuple() lists them in declaration order
    # (Location, ImpactPoint, Normal, ... bBlockingHit, ... Component), so pick them by type.
    result = hit[1] if isinstance(hit, tuple) else hit
    fields = result.to_tuple() if result is not None else ()
    vectors = [f for f in fields if isinstance(f, unreal.Vector)]
    flags = [f for f in fields if isinstance(f, bool)]
    if (isinstance(hit, tuple) and not hit[0]) or not flags or not flags[0] or len(vectors) < 2:
        RESULT["warnings"].append("no floor under (%d, %d); using 0" % (x, y))
        return 0.0, None
    components = [f for f in fields if isinstance(f, unreal.PrimitiveComponent)]
    label = components[0].get_owner().get_actor_label() if components else None
    return vectors[1].z, label


def find_spawn(world):
    """First flat open floor point: same floor and height within 5 cm under the point and 80 cm around it."""
    for x in SPAWN_SEARCH_X:
        for y in SPAWN_SEARCH_Y:
            z, floor = ground_z(world, x, y)
            if not floor or not floor.startswith(SPAWN_FLOORS) or abs(z) > 100.0:
                continue
            around = [ground_z(world, x + dx, y + dy) for dx, dy in ((80, 0), (-80, 0), (0, 80), (0, -80))]
            if all(f and f.startswith(SPAWN_FLOORS) and abs(h - z) < 5.0 for h, f in around):
                return (float(x), float(y), z), floor
    raise RuntimeError("No open plaza floor found in front of the main building")


def spawn(cls, location, yaw=0.0, label=None):
    actor = ACTORS.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, yaw))
    require(actor, "Could not spawn " + str(cls))
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path("Lobby")
    return actor


def strip_match_actors():
    removed = {"court": 0, "crowd": 0, "player_start": 0}
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        if isinstance(actor, unreal.PlayerStart):
            key = "player_start"
        elif label == "Court" or "JGCourt" in actor.get_class().get_name():
            key = "court"
        elif isinstance(actor, unreal.SkeletalMeshActor) and str(actor.get_folder_path()).startswith("Crowd"):
            key = "crowd"
        else:
            continue
        require(ACTORS.destroy_actor(actor), "Could not delete " + label)
        removed[key] += 1
    RESULT["lobby_removed"] = removed


def find_by_label_prefix(prefix):
    return [a for a in ACTORS.get_all_level_actors() if a.get_actor_label().startswith(prefix)]


def place_zones(world):
    zones = []
    z, floor = ground_z(world, *MATCH_ZONE_XY)
    match_zone = spawn(unreal.JGInteractZone, (MATCH_ZONE_XY[0], MATCH_ZONE_XY[1], z + 120.0), label="Zone_Match")
    match_zone.set_editor_property("kind", unreal.JGZoneKind.MATCH)
    zones.append({"kind": "Match", "location": [MATCH_ZONE_XY[0], MATCH_ZONE_XY[1], round(z)], "floor": floor})

    halls = find_by_label_prefix("SM_C360_StudentHall")
    if not halls:
        RESULT["warnings"].append("no SM_C360_StudentHall: settings zone skipped (shortcut still opens the panel)")
        return zones
    origin, extent = halls[0].get_actor_bounds(False)
    # entrance side = the side facing the court; step out of the building along the dominant axis
    if abs(origin.x) >= abs(origin.y):
        x, y = origin.x - math.copysign(extent.x + 250.0, origin.x), origin.y
    else:
        x, y = origin.x, origin.y - math.copysign(extent.y + 250.0, origin.y)
    z, floor = ground_z(world, x, y)
    settings_zone = spawn(unreal.JGInteractZone, (x, y, z + 120.0), label="Zone_Settings")
    settings_zone.set_editor_property("kind", unreal.JGZoneKind.SETTINGS)
    zones.append({"kind": "Settings", "location": [round(x), round(y), round(z)], "floor": floor,
                  "hall_center": [round(origin.x), round(origin.y)]})
    return zones


def place_walls(site):
    cube = EAL.load_asset("/Engine/BasicShapes/Cube")
    require(cube, "Missing /Engine/BasicShapes/Cube")
    x0, x1, y0, y1 = site
    walls = [("North", ((x0 + x1) / 2, y1), (x1 - x0, WALL_THICKNESS)),
             ("South", ((x0 + x1) / 2, y0), (x1 - x0, WALL_THICKNESS)),
             ("East", (x1, (y0 + y1) / 2), (WALL_THICKNESS, y1 - y0)),
             ("West", (x0, (y0 + y1) / 2), (WALL_THICKNESS, y1 - y0))]
    for name, (x, y), (sx, sy) in walls:
        actor = spawn(unreal.StaticMeshActor, (x, y, WALL_HEIGHT / 2), label="LobbyBoundary_" + name)
        actor.static_mesh_component.set_static_mesh(cube)
        # the basic cube is 100 cm on each side
        actor.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, WALL_HEIGHT / 100.0))
        actor.set_actor_hidden_in_game(True)
        component = actor.static_mesh_component
        component.set_collision_profile_name("BlockAll")
        for channel in (unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionChannel.ECC_VISIBILITY, BALL_CHANNEL):
            component.set_collision_response_to_channel(channel, unreal.CollisionResponseType.ECR_IGNORE)
        component.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
        component.set_editor_property("cast_shadow", False)
    RESULT["walls"] = len(walls)


def place_nav_bounds(site):
    x0, x1, y0, y1 = site
    z0, z1 = NAV_HEIGHT
    volume = spawn(unreal.NavMeshBoundsVolume, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), label="NavBounds_School")
    # the default volume brush is a 200 cm cube
    volume.set_actor_scale3d(unreal.Vector((x1 - x0) / 200.0, (y1 - y0) / 200.0, (z1 - z0) / 200.0))
    origin, extent = volume.get_actor_bounds(False)
    RESULT["nav_bounds"] = {"origin": [round(origin.x), round(origin.y), round(origin.z)],
                            "extent": [round(extent.x), round(extent.y), round(extent.z)]}
    require(extent.x > 1000.0 and extent.y > 1000.0, "NavMeshBoundsVolume has no brush (extent %s)" % extent)


def flat_floor(world, x, y, walk):
    """Floor height at (x, y) if it is a listed walkable floor and flat within 80 cm around, else None."""
    z, floor = ground_z(world, x, y)
    if not floor or not any(floor == base or floor.startswith(base + "_") for base in walk):
        return None
    for dx, dy in ((80, 0), (-80, 0), (0, 80), (0, -80)):
        h, f = ground_z(world, x + dx, y + dy)
        if not f or abs(h - z) > 10.0:
            return None
    return z


def place_students(world, site, walk, spawn_xy):
    """Spread the students over flat floor: farthest-point picks from a grid, away from the court and spawn."""
    (cx0, cx1, cy0, cy1), spawn_clear = STUDENT_KEEP_CLEAR
    x0, x1, y0, y1 = site
    warnings_before = len(RESULT["warnings"])
    candidates = []
    for x in range(int(x0) + 300, int(x1) - 300, STUDENT_GRID_CM):
        for y in range(int(y0) + 300, int(y1) - 300, STUDENT_GRID_CM):
            if cx0 <= x <= cx1 and cy0 <= y <= cy1:
                continue
            if math.hypot(x - spawn_xy[0], y - spawn_xy[1]) < spawn_clear:
                continue
            z = flat_floor(world, x, y, walk)
            if z is not None:
                candidates.append((x, y, z))
    # the grid probes miss roofs and blockers on purpose; their "no floor" warnings are not news
    del RESULT["warnings"][warnings_before:]
    require(len(candidates) >= len(STUDENT_LOOKS), "Only %d floor points for students" % len(candidates))

    # deterministic spread: start nearest the spawn, then always take the point farthest from those taken
    picks = [min(candidates, key=lambda c: math.hypot(c[0] - spawn_xy[0], c[1] - spawn_xy[1]))]
    while len(picks) < len(STUDENT_LOOKS):
        picks.append(max(candidates, key=lambda c: min(math.hypot(c[0] - p[0], c[1] - p[1]) for p in picks)))

    emotes = [EAL.load_asset(RIG + "/Montages/AM_DJG_" + name) for name in STUDENT_EMOTES]
    require(all(emotes), "Missing an emote montage")
    placed = []
    for index, ((x, y, z), look) in enumerate(zip(picks, STUDENT_LOOKS)):
        data = EAL.load_asset(RIG + "/Data/DA_DJG_" + look)
        require(data, "Missing DA_DJG_" + look)
        # capsule half height 96 above the floor; face a random-looking but fixed direction
        student = spawn(unreal.JGStudentCharacter, (x, y, z + 98.0), yaw=(index * 137.0) % 360.0,
                        label="Student_%02d" % (index + 1))
        student.set_folder_path("Lobby/Students")
        student.set_editor_property("look_data", data)
        student.set_editor_property("emote_montages", emotes)
        placed.append({"label": "Student_%02d" % (index + 1), "look": look, "location": [x, y, round(z)]})
    RESULT["students"] = {"candidates": len(candidates), "placed": placed}


def build_match():
    copy_map(MATCH)
    world = open_map(MATCH)
    set_game_mode(world, "/Game/Jokgu/Core/BP_JGGameMode")
    save(world, MATCH)


def build_lobby(site, walk):
    copy_map(LOBBY)
    world = open_map(LOBBY)
    set_game_mode(world, "/Game/Jokgu/Lobby/BP_JGLobbyGameMode")
    strip_match_actors()
    (x, y, z), floor = find_spawn(world)
    spawn(unreal.PlayerStart, (x, y, z + 100.0), yaw=SPAWN_YAW, label="PlayerStart_Lobby")
    RESULT["spawn"] = {"location": [x, y, round(z)], "floor": floor}
    RESULT["zones"] = place_zones(world)
    place_walls(site)
    place_nav_bounds(site)
    place_students(world, site, walk, (x, y))
    save(world, LOBBY)


def verify(path, expected_mode):
    # world settings did not always survive a commandlet save (collision did not, 10/12), so read them back
    world = open_map(path)
    mode = world.get_world_settings().get_editor_property("default_game_mode")
    name = mode.get_name() if mode else None
    RESULT.setdefault("verified", {})[path] = name
    require(name == expected_mode, "%s reloaded with GameMode %s" % (path, name))


def main():
    collision = json.loads(EDITS.read_text(encoding="utf-8"))["collision"]
    build_match()
    build_lobby(collision["site_cm"], collision["walk"])
    verify(MATCH, "BP_JGGameMode_C")
    verify(LOBBY, "BP_JGLobbyGameMode_C")
    RESULT["success"] = True


def run():
    try:
        main()
    except Exception:
        RESULT["error"] = traceback.format_exc()
    REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("CAMPUS_LOBBY_MAPS " + ("OK" if RESULT["success"] else "FAILED"))
    return RESULT["success"]


if __name__ == "__main__":
    run()
