"""Blender (5.x, background) analysis of the CampusEnclosed GLB that Unreal imports.

Finds background objects that no lobby walker or match camera can see, and close tree pairs.
Writes Tools/Campus/Reports/analysis_report.json. It never edits anything: cleanup_campus_level.py
reads the candidate lists only after they are copied into campus_edits.json.

blender -b --python Tools/Campus/analyze_campus.py -- <glb path>

Names: GLB node "SM_DJG_Bench.020" is Unreal actor label "SM_DJG_Bench_020".
Axes: Blender metres (bx, by, bz) are Unreal centimetres (100*by, 100*bx, 100*bz).
"""
import json
import math
import random
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector

REPORT = Path(__file__).resolve().parent / "Reports/analysis_report.json"

SITE = (-60.3, 60.3, -30.35, 52.22)  # school wall, Blender metres: x min/max, y min/max
GRID_STEP = 6.0                       # walker sample spacing inside the wall
WALK_MAX_Z = 3.2                      # higher first hit below a sample = roof, not a walkable spot
EYE_HEIGHTS = (1.7, 3.5)              # walker eyes and third-person camera
# match cameras: -42 deg, 26 m from the focus 1.05 m above the net, one per team, plus lateral tracking
MATCH_CAMERAS = [(lx, side * 26.0 * math.cos(math.radians(42)), 1.05 + 26.0 * math.sin(math.radians(42)))
                 for side in (-1, 1) for lx in (-3.0, 0.0, 3.0)]
SAMPLES_PER_OBJECT = 10
KEEP_SIZE = 40.0                      # objects wider than this are terrain/roads/water: never candidates
KEEP_TOKENS = ("Court", "Net", "JokguBall", "Ground", "Terrain", "Water", "Ripples", "Road", "Sidewalk",
               "Embankment", "Plot", "Boundary")
TREE_TOKENS = ("Tree",)               # SM_DJG_Tree_*, SM_C360_CherryTree, SM_ENC_Tree*; not the Forest belt
TREE_MIN_SPACING = 2.5


def label(obj):
    return obj.name.replace(".", "_")


def to_unreal(v):
    return [round(100.0 * v.y), round(100.0 * v.x), round(100.0 * v.z)]


def world_bounds(obj):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    return lo, hi


def triangles(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def surface_samples(obj, count, rng):
    verts = obj.data.vertices
    if not len(verts):
        return []
    picks = [verts[rng.randrange(len(verts))].co for _ in range(count)]
    lo, hi = world_bounds(obj)
    return [(lo + hi) * 0.5] + [obj.matrix_world @ v for v in picks]


def viewpoints(scene, depsgraph):
    points, skipped = [], 0
    x0, x1, y0, y1 = SITE
    x = x0 + GRID_STEP * 0.5
    while x < x1:
        y = y0 + GRID_STEP * 0.5
        while y < y1:
            hit, loc, _, _, _, _ = scene.ray_cast(depsgraph, Vector((x, y, 60.0)), Vector((0, 0, -1)), distance=120.0)
            if hit and loc.z <= WALK_MAX_Z:
                points += [Vector((x, y, loc.z + h)) for h in EYE_HEIGHTS]
            else:
                skipped += 1
            y += GRID_STEP
        x += GRID_STEP
    points += [Vector(c) for c in MATCH_CAMERAS]
    return points, skipped


def visible(scene, depsgraph, obj, eyes, targets):
    for target in targets:
        # nearest viewpoints first: visible objects exit early
        for eye in sorted(eyes, key=lambda e: (e - target).length_squared):
            direction = target - eye
            dist = direction.length
            if dist < 0.05:
                return True
            direction.normalize()
            hit, _, _, _, hit_obj, _ = scene.ray_cast(depsgraph, eye, direction, distance=dist - 0.05)
            if not hit or hit_obj.name == obj.name:
                return True
    return False


def close_trees(objs):
    trees = []
    for obj in objs:
        if any(t in obj.name for t in TREE_TOKENS) and "Forest" not in obj.name:
            lo, hi = world_bounds(obj)
            trees.append((obj, obj.matrix_world.translation, (hi.x - lo.x) * (hi.y - lo.y) * (hi.z - lo.z)))
    pairs = []
    for i, (a, pa, va) in enumerate(trees):
        for b, pb, vb in trees[i + 1:]:
            d = math.hypot(pa.x - pb.x, pa.y - pb.y)
            if d < TREE_MIN_SPACING:
                keep, drop = (a, b) if va >= vb else (b, a)
                pairs.append({"distance_m": round(d, 2), "keep": label(keep), "remove": label(drop),
                              "remove_location_cm": to_unreal(drop.matrix_world.translation)})
    return pairs


def main():
    glb = sys.argv[sys.argv.index("--") + 1]
    started = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=glb)
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()
    objs = [o for o in scene.objects if o.type == "MESH"]
    eyes, skipped = viewpoints(scene, depsgraph)
    rng = random.Random(7)

    total_tris = 0
    candidates, kept = [], 0
    for obj in objs:
        tris = triangles(obj)
        total_tris += tris
        lo, hi = world_bounds(obj)
        if max(hi.x - lo.x, hi.y - lo.y) > KEEP_SIZE or any(t in obj.name for t in KEEP_TOKENS):
            kept += 1
            continue
        if not visible(scene, depsgraph, obj, eyes, surface_samples(obj, SAMPLES_PER_OBJECT, rng)):
            candidates.append({"label": label(obj), "mesh": obj.data.name, "triangles": tris,
                               "location_cm": to_unreal((lo + hi) * 0.5)})

    by_mesh = {}
    for c in candidates:
        entry = by_mesh.setdefault(c["mesh"], [0, 0])
        entry[0] += 1
        entry[1] += c["triangles"]
    removed_tris = sum(c["triangles"] for c in candidates)
    pairs = close_trees(objs)
    report = {
        "source": glb,
        "seconds": round(time.time() - started, 1),
        "objects": len(objs),
        "triangles": total_tris,
        "viewpoints": len(eyes),
        "grid_cells_skipped_as_roof": skipped,
        "never_tested_large_or_kept": kept,
        "invisible_count": len(candidates),
        "invisible_triangles": removed_tris,
        "triangles_after_invisible_removal": total_tris - removed_tris,
        "invisible_by_mesh": dict(sorted(by_mesh.items(), key=lambda kv: -kv[1][1])),
        "close_tree_pairs": pairs,
        "invisible": candidates,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("CAMPUS_ANALYSIS invisible=%d tris=%d/%d pairs=%d" % (len(candidates), removed_tris, total_tris, len(pairs)))


main()
