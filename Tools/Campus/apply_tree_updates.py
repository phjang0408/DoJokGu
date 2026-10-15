"""UE 5.8 Python commandlet step (2026-10-15): tree branch fix + trunk collision, without rebuilding any map.

1. Replaces the geometry of the 4 trees edited by Art/Schoolyard/CampusEnclosed/connect_tree_branches.py with
   Exports/Trees/*.glb (imported into /Game/Temp, copied into the existing assets, temp folder deleted), so every placed
   actor updates. Material slots and Nanite settings are kept.
2. Gives every tree mesh in Reports/tree_trunks.json one trunk capsule (simple collision, leaves never block).
3. In L_Campus_Lobby (hand-edited, backed up first) and L_Campus_Match, sets the "trunk" trees inside the wall to
   collide like the other blockers (cleanup_campus_level.make_collidable). L_CampusEnclosed gets the same on its next
   cleanup run (campus_edits.json collision.trunk).
Writes Tools/Campus/Reports/tree_updates_report.json.

`-script="apply_tree_updates.py --meshes-only"` (10/15 branch UV fix) does steps 1-2 for the 4 edited trees only and
leaves the maps alone.
"""
import json
import re
import shutil
import sys
import traceback
from pathlib import Path
import unreal

TOOLS = Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"
sys.path.insert(0, str(TOOLS))
from cleanup_campus_level import inside, is_tree, make_collidable  # noqa: E402

PROJECT = Path(unreal.Paths.project_dir()).resolve()
TREES = PROJECT / "Art/Schoolyard/CampusEnclosed"
MESH_DIR = "/Game/Jokgu/Environment/CampusEnclosed/DJG_CampusEnclosed/StaticMeshes"
TEMP_DIR = "/Game/Temp/TreeImport"
MAPS = ["/Game/Jokgu/Maps/L_Campus_Lobby", "/Game/Jokgu/Maps/L_Campus_Match"]
REPORT = TOOLS / "Reports/tree_updates_report.json"
EAL = unreal.EditorAssetLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
RESULT = {"success": False, "reimported": {}, "trunks": {}, "maps": {}}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def slot_key(name):
    return re.sub("[^a-z0-9]", "", str(name).lower())


def triangle_count(mesh):
    # render data is missing under -nullrhi (Nanite), so count the source mesh description
    return mesh.get_static_mesh_description(0).get_triangle_count()


def reimport(asset):
    """Import the GLB into a temp folder and copy its mesh description into the existing asset (same object, so
    placed actors keep their reference). A replace-existing import found nothing to import: the asset keeps the
    pipeline of the original whole-campus scene import."""
    path = MESH_DIR + "/" + asset
    mesh = EAL.load_asset(path)
    require(mesh, "Missing " + path)
    materials = {slot_key(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface")
                 for m in mesh.get_editor_property("static_materials")}
    nanite = mesh.get_editor_property("nanite_settings")
    tris_before = triangle_count(mesh)

    temp = TEMP_DIR + "/" + asset
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(TREES / "Exports/Trees" / (asset + ".glb")))
    task.set_editor_property("destination_path", temp)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [EAL.load_asset(p) for p in EAL.list_assets(temp, recursive=True, include_folder=False)]
    sources = [a for a in imported if isinstance(a, unreal.StaticMesh)]
    require(len(sources) == 1, asset + ": the GLB import made static meshes " + str([a.get_path_name() for a in sources]))
    description = sources[0].get_static_mesh_description(0)
    require(description.get_triangle_count() >= tris_before, asset + ": imported mesh has %d triangles, old %d"
            % (description.get_triangle_count(), tris_before))

    mesh.build_from_static_mesh_descriptions([description], False)
    slots = mesh.get_editor_property("static_materials")
    for slot in slots:
        key = slot_key(slot.get_editor_property("imported_material_slot_name") or slot.get_editor_property("material_slot_name"))
        require(key in materials, asset + ": unknown material slot " + key)
        slot.set_editor_property("material_interface", materials[key])
    mesh.set_editor_property("static_materials", slots)
    mesh.set_editor_property("nanite_settings", nanite)
    tris_after = triangle_count(mesh)
    require(tris_after == description.get_triangle_count(), asset + ": mesh description was not copied")
    RESULT["reimported"][asset] = {"triangles_before": tris_before, "triangles_after": tris_after, "slots": len(slots),
                                   "materials": [str(s.get_editor_property("material_interface").get_name()) for s in slots]}
    return mesh


def set_trunk(asset, capsule):
    mesh = EAL.load_asset(MESH_DIR + "/" + asset)
    require(mesh, "Missing " + asset)
    body = mesh.get_editor_property("body_setup")
    require(body, asset + ": no body setup")
    geom = unreal.KAggregateGeom()  # replaces any earlier shapes
    elem = unreal.KSphylElem()
    radius, height = capsule["radius_cm"], capsule["height_cm"]
    elem.set_editor_property("center", unreal.Vector(*capsule["center_cm"]))
    elem.set_editor_property("rotation", unreal.Rotator(0, 0, 0))
    elem.set_editor_property("radius", radius)
    elem.set_editor_property("length", max(height - 2 * radius, 1.0))
    geom.set_editor_property("sphyl_elems", [elem])
    body.set_editor_property("agg_geom", geom)
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    check = body.get_editor_property("agg_geom").get_editor_property("sphyl_elems")[0]
    require(abs(check.get_editor_property("radius") - radius) < .1, asset + ": capsule radius did not stick")
    require(EAL.save_loaded_asset(mesh, False), "Could not save " + asset)
    RESULT["trunks"][asset] = {"radius_cm": radius, "length_cm": check.get_editor_property("length"),
                               "center_cm": capsule["center_cm"]}


def apply_map(level, rules):
    source = PROJECT / ("Content" + level[len("/Game"):] + ".umap")
    backup = source.with_name(source.stem + "_before_tree_collision.umap.bak")
    if not backup.exists():
        shutil.copy2(source, backup)
    require(unreal.EditorLoadingAndSavingUtils.load_map(level), "Could not load " + level)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world.get_path_name().startswith(level), "Editor world is not " + level)
    counts = {"trunk_inside": 0, "trunk_outside": 0}
    for actor in ACTORS.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor) or not is_tree(actor, rules["trunk"]):
            continue
        if inside(actor, rules["site_cm"]):
            make_collidable(actor, False, {}, complex_collision=False)
            counts["trunk_inside"] += 1
        else:
            counts["trunk_outside"] += 1
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, level), "Could not save " + level)
    RESULT["maps"][level] = dict(counts, backup=str(backup.relative_to(PROJECT)))


def main():
    meshes_only = "--meshes-only" in sys.argv
    trunks = json.loads((TREES / "Reports/tree_trunks.json").read_text(encoding="utf-8"))["trunks"]
    edited = ("SM_DJG_Tree_A_ExportMesh_001", "SM_DJG_Tree_A_ExportMesh_002",
              "SM_DJG_Tree_B_ExportMesh_001", "SM_DJG_Tree_C_ExportMesh_001")
    for asset in edited:
        mesh = reimport(asset)
        require(EAL.save_loaded_asset(mesh, False), "Could not save " + asset)
    EAL.delete_directory(TEMP_DIR)
    for asset, capsule in trunks.items():
        if not meshes_only or asset in edited:
            set_trunk(asset, capsule)
    if meshes_only:
        RESULT["success"] = True
        return
    rules = json.loads((TOOLS / "campus_edits.json").read_text(encoding="utf-8"))["collision"]
    for level in MAPS:
        apply_map(level, rules)
    RESULT["success"] = True


try:
    main()
except Exception:
    RESULT["error"] = traceback.format_exc()
REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2, default=str), encoding="utf-8")
unreal.log("TREE_UPDATES " + ("OK" if RESULT["success"] else "FAILED"))
