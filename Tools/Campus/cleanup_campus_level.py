"""UE 5.8 Python commandlet step: applies campus_edits.json to /Game/Jokgu/Maps/L_CampusEnclosed.

Runs right after build_campus_level.py (see rebuild_campus.py), so every edit survives a rebuild.
Actors are found by label (GLB node name with "." -> "_"). Edits:
- delete: labels to remove, grouped by reason
- transform: per label, optional location [x, y, z] cm, yaw deg, uniform scale
- add: new StaticMeshActors {label, mesh, location, yaw, scale}
- materials: material overrides by mesh base name; a missing vertex-color material is created
  (the terrain's height tint is GLB vertex colour, which the imported material ignores), and a missing
  river material (world-aligned panning water normals) is created for the river surface
- traffic: closed lane loops around the school road. The sedans standing in those lanes become the moving
  cars of an AJGTrafficLoop (their mesh and pose relative to the lane) and the static copies are removed
- collision: mesh base names (label == base or base_<n>) for walkable floors and blockers inside the wall. Pawns collide; the
  JGBall channel and the camera channel are ignored so the ball and match camera are unaffected. "trunk" tree meshes
  inside the wall block with the trunk capsule that apply_tree_updates.py puts on the mesh (leaves never block).
  Every other StaticMeshActor (outside buildings, trees and blockers outside the wall) is set to NoCollision.
Writes Tools/Campus/Reports/cleanup_report.json.
"""
import json
import math
import traceback
from pathlib import Path
import unreal

TOOLS = Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"
EDITS = TOOLS / "campus_edits.json"
REPORT = TOOLS / "Reports/cleanup_report.json"
LEVEL = "/Game/Jokgu/Maps/L_CampusEnclosed"
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EAL = unreal.EditorAssetLibrary
BALL_CHANNEL = unreal.CollisionChannel.ECC_JG_BALL  # GameTraceChannel1, named "JGBall" in DefaultEngine.ini
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


def matches(name, bases):
    return any(name == base or name.startswith(base + "_") for base in bases)


def ensure_vertex_color_material(path, roughness):
    if EAL.does_asset_exist(path):
        material = EAL.load_asset(path)
        if not material.get_editor_property("used_with_nanite"):  # assets made before this flag was set
            material.set_editor_property("used_with_nanite", True)
            unreal.MaterialEditingLibrary.recompile_material(material)
            require(EAL.save_loaded_asset(material, False), "Could not save " + path)
        return material
    folder, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(material, "Could not create " + path)
    # the imported background meshes are Nanite; without this flag cooked builds fall back to the default material
    material.set_editor_property("used_with_nanite", True)
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    require(EAL.save_loaded_asset(material, False), "Could not save " + path)
    return material


def ensure_river_material(path, river):
    """Opaque water: world-aligned water normals panning along the flow at two scales, fresnel-tinted colour."""
    if EAL.does_asset_exist(path):
        return EAL.load_asset(path)
    # EditorAssetLibrary.load_asset returns None for this engine texture by package path; unreal.load_asset works
    normal_texture = unreal.load_asset(river["normal_texture"])
    require(normal_texture, "Missing " + river["normal_texture"])
    folder, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(material, "Could not create " + path)
    material.set_editor_property("used_with_nanite", True)
    mel = unreal.MaterialEditingLibrary

    def node(cls, x, y, **props):
        expression = mel.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    # the river mesh UVs do not follow the flow, so sample in world XY (cm) instead
    world = node(unreal.MaterialExpressionWorldPosition, -1400, 0)
    world_xy = node(unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=True, b=False, a=False)
    mel.connect_material_expressions(world, "", world_xy, "")
    flow_x, flow_y = river["flow_direction"]
    normals = []
    for index, (tile_cm, speed) in enumerate(river["layers"]):
        scale = node(unreal.MaterialExpressionConstant, -1200, 150 + 300 * index, r=1.0 / tile_cm)
        uv = node(unreal.MaterialExpressionMultiply, -1000, 300 * index)
        mel.connect_material_expressions(world_xy, "", uv, "A")
        mel.connect_material_expressions(scale, "", uv, "B")
        # panner speed is in UV per second; one UV = tile_cm, so the water moves `speed` cm/s along the flow
        panner = node(unreal.MaterialExpressionPanner, -800, 300 * index,
                      speed_x=flow_x * speed / tile_cm, speed_y=flow_y * speed / tile_cm)
        mel.connect_material_expressions(uv, "", panner, "Coordinate")
        sample = node(unreal.MaterialExpressionTextureSample, -550, 300 * index,
                      texture=normal_texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(panner, "", sample, "UVs")
        normals.append(sample)
    blended = node(unreal.MaterialExpressionAdd, -300, 150)
    mel.connect_material_expressions(normals[0], "RGB", blended, "A")
    mel.connect_material_expressions(normals[1], "RGB", blended, "B")
    mel.connect_material_property(blended, "", unreal.MaterialProperty.MP_NORMAL)

    deep = node(unreal.MaterialExpressionConstant3Vector, -550, -400,
                constant=unreal.LinearColor(*river["deep_color"], 1.0))
    grazing = node(unreal.MaterialExpressionConstant3Vector, -550, -250,
                   constant=unreal.LinearColor(*river["grazing_color"], 1.0))
    fresnel = node(unreal.MaterialExpressionFresnel, -550, -100)
    color = node(unreal.MaterialExpressionLinearInterpolate, -300, -300)
    mel.connect_material_expressions(deep, "", color, "A")
    mel.connect_material_expressions(grazing, "", color, "B")
    mel.connect_material_expressions(fresnel, "", color, "Alpha")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionConstant, -300, 450, r=river["roughness"])
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    require(EAL.save_loaded_asset(material, False), "Could not save " + path)
    return material


def apply_materials(edits, actors):
    done = 0
    for rule in edits.get("materials", []):
        if "vertex_color_roughness" in rule:
            material = ensure_vertex_color_material(rule["material"], rule["vertex_color_roughness"])
        elif "river" in rule:
            material = ensure_river_material(rule["material"], rule["river"])
        else:
            material = EAL.load_asset(rule["material"])
        require(material, "Missing material " + rule["material"])
        for name, actor in actors.items():
            if isinstance(actor, unreal.StaticMeshActor) and matches(name, rule["bases"]):
                component = actor.static_mesh_component
                for slot in range(component.get_num_materials()):
                    component.set_material(slot, material)
                done += 1
    RESULT["materials"] = done


def lane_points(rect, radius, south_edge_direction, z):
    """Closed lane around rect [x min, x max, y min, y max] with rounded corners, in driving order.
    "+y" drives the x-min (south) edge toward +y; "-y" is the opposite way round."""
    x0, x1, y0, y1 = rect
    # corner centres and arc angles, going x-min/y-max -> x-max/y-max -> x-max/y-min -> x-min/y-min
    corners = [((x0 + radius, y1 - radius), 180, 90),
               ((x1 - radius, y1 - radius), 90, 0),
               ((x1 - radius, y0 + radius), 0, -90),
               ((x0 + radius, y0 + radius), -90, -180)]
    points = []
    for (cx, cy), start, end in corners:
        for step in range(5):
            angle = math.radians(start + (end - start) * step / 4)
            points.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle), z))
    # this order runs the x-min edge from y-min to y-max last, i.e. toward +y
    if south_edge_direction == "-y":
        points.reverse()
    return points


def flat_path_transform(path, distance):
    """Spline frame at distance with yaw only, like AJGTrafficLoop::UpdateCarTransforms."""
    frame = path.get_transform_at_distance_along_spline(distance, unreal.SplineCoordinateSpace.WORLD, False)
    yaw = frame.rotation.rotator().yaw
    return unreal.Transform(frame.translation, unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0))


def apply_traffic(edits, actors):
    rules = edits.get("traffic")
    if not rules:
        return
    loops = []
    for rule in rules["loops"]:
        loop = actors.get(rule["label"])
        if loop is None:
            loop = ACTORS.spawn_actor_from_class(unreal.JGTrafficLoop, unreal.Vector(0.0, 0.0, 0.0))
            require(loop, "Could not spawn " + rule["label"])
            loop.set_actor_label(rule["label"])
            loop.set_folder_path("Ambient")
        path = loop.get_component_by_class(unreal.SplineComponent)
        path.clear_spline_points(False)
        for point in lane_points(rule["rect_cm"], rule["corner_radius_cm"], rule["south_edge_direction"], rules["road_z"]):
            path.add_spline_point(unreal.Vector(*point), unreal.SplineCoordinateSpace.WORLD, False)
        path.set_closed_loop(True, False)
        path.update_spline()
        cars, report = [], []
        for name in rule["cars"]:
            actor = actors.pop(name, None)
            if actor is None:
                # a re-run on an already cleaned map: the cars were moved into the loop last time
                RESULT["warnings"].append("traffic: no actor " + name)
                continue
            key = path.find_input_key_closest_to_world_location(actor.get_actor_location())
            distance = path.get_distance_along_spline_at_spline_input_key(key)
            offset = unreal.MathLibrary.make_relative_transform(actor.get_actor_transform(), flat_path_transform(path, distance))
            car = unreal.JGTrafficCar()
            car.set_editor_property("mesh", actor.static_mesh_component.static_mesh)
            car.set_editor_property("start_distance", distance)
            car.set_editor_property("mesh_offset", offset)
            cars.append(car)
            lane_gap = offset.translation.y
            report.append({"car": name, "distance": round(distance), "lane_gap_cm": round(lane_gap),
                           "yaw_offset": round(offset.rotation.rotator().yaw)})
            # a car more than half a lane from the path line is not in this lane: the rect is wrong
            require(abs(lane_gap) < 100.0, "%s is %.0f cm off the %s lane" % (name, lane_gap, rule["label"]))
            require(ACTORS.destroy_actor(actor), "Could not delete " + name)
        if cars:  # a re-run finds no cars left and keeps the ones from the first run
            loop.set_editor_property("cars", cars)
        loop.set_editor_property("speed", rules["speed_cm_s"])
        actors[rule["label"]] = loop
        loops.append({"label": rule["label"], "length_cm": round(path.get_spline_length()), "cars": report})
    # the sedan meshes share one forward axis, so every car needs the same yaw offset from its lane;
    # a 180 deg split means a loop runs against the way its cars were parked
    yaws = sorted({c["yaw_offset"] % 360 for loop in loops for c in loop["cars"]})
    require(not yaws or max(min(abs(y - yaws[0]), 360 - abs(y - yaws[0])) for y in yaws) < 10,
            "traffic: cars disagree on the driving direction " + str(yaws))
    RESULT["traffic"] = loops


def inside(actor, site):
    location = actor.get_actor_location()
    return site[0] <= location.x <= site[1] and site[2] <= location.y <= site[3]


def make_collidable(actor, walkable, meshes, complex_collision=True):
    component = actor.static_mesh_component
    mesh = component.static_mesh
    if complex_collision and mesh and mesh.get_path_name() not in meshes:
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


def is_tree(actor, bases):
    # by mesh, not label: copies placed by hand in the lobby keep labels like SM_ENC_Tree3 on an SM_ENC_Tree0 mesh
    mesh = actor.static_mesh_component.static_mesh
    return bool(mesh) and any(mesh.get_name().startswith(base + "_ExportMesh") for base in bases)


def make_uncollidable(actor):
    component = actor.static_mesh_component
    # the profile name is what survives a save; set_collision_enabled alone left 1,981 actors at QueryAndPhysics (10/12)
    component.set_collision_profile_name("NoCollision")
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)


def apply_collision(edits, actors):
    rules = edits.get("collision", {})
    site = rules.get("site_cm")
    require(site and len(site) == 4, "collision.site_cm must be [x min, x max, y min, y max]")
    meshes, counts = {}, {"walk": 0, "block": 0, "trunk": 0, "none": 0}
    for name, actor in actors.items():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        kind = next((k for k in ("walk", "block") if matches(name, rules.get(k, []))), None)
        if kind is None and is_tree(actor, rules.get("trunk", [])):
            kind = "trunk"
        # floors may extend past the wall (terrain, plazas); blockers only matter inside it
        if kind == "walk" or (kind in ("block", "trunk") and inside(actor, site)):
            # trunks keep the mesh's simple capsule instead of colliding with every leaf
            make_collidable(actor, kind == "walk", meshes, complex_collision=kind != "trunk")
            counts[kind] += 1
        else:
            make_uncollidable(actor)
            counts["none"] += 1
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
    apply_materials(edits, actors)
    apply_traffic(edits, actors)
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
