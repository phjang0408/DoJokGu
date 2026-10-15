"""UE 5.8 Python commandlet: import the DJG common rig into a dedicated content folder.

The existing game Blueprint and character data are not changed by this script.
Run again to update generated content. Missing optional clips produce warnings.
"""
import json
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve() / "Art/Characters/Rigged"
DEST = "/Game/Jokgu/Characters/Rigged"
REPORT = ROOT / "Reports/unreal_import_report.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
RESULT = {"engine": unreal.SystemLibrary.get_engine_version(), "meshes": [],
          "animations": [], "montages": [], "data_assets": [], "warnings": [], "success": False,
          "runtime_integration_ready": hasattr(unreal, "JGAnimInstance") and hasattr(unreal, "JGRigAnimationLibrary")}
REQUIRED = {"Idle", "Walk", "Run", "Tap", "WeakKick", "MediumKick", "StrongKick", "Serve", "Slide"}
MONTAGE_FIELDS = {"Tap": "tap_montage", "WeakKick": "weak_kick_montage",
                  "MediumKick": "medium_kick_montage", "StrongKick": "strong_kick_montage",
                  "Serve": "serve_montage", "Slide": "slide_montage"}


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def import_asset(path, folder, options=None, factory=None):
    task = unreal.AssetImportTask()
    task.set_editor_properties({"filename": str(path), "destination_path": folder,
                               "destination_name": path.stem, "automated": True,
                               "replace_existing": True, "replace_existing_settings": True,
                               "save": True})
    if options:
        task.options = options
    if factory:
        task.factory = factory
    TOOLS.import_asset_tasks([task])
    asset = EAL.load_asset(folder + "/" + path.stem)
    if not asset:
        raise RuntimeError("Import did not produce " + folder + "/" + path.stem)
    return asset


def fbx_options(skeleton=None, animation=False):
    options = unreal.FbxImportUI()
    options.set_editor_properties({"automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "import_as_skeletal": True, "import_mesh": not animation, "import_animations": animation,
        "import_materials": False, "import_textures": False, "create_physics_asset": False})
    if skeleton:
        options.skeleton = skeleton
    data = options.anim_sequence_import_data if animation else options.skeletal_mesh_import_data
    data.set_editor_properties({"convert_scene": True, "convert_scene_unit": True,
                               "force_front_x_axis": True, "import_uniform_scale": 1.0})
    if animation:
        data.set_editor_properties({"animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
            "use_default_sample_rate": True, "custom_sample_rate": 30, "import_bone_tracks": True})
    else:
        data.set_editor_properties({"update_skeleton_reference_pose": False, "use_t0_as_ref_pose": False,
            "import_morph_targets": False, "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS})
    return options


def material():
    palette = import_asset(ROOT / "Textures/T_DJG_Palette.png", DEST + "/Textures")
    roughness = import_asset(ROOT / "Textures/T_DJG_Roughness.png", DEST + "/Textures")
    # Blender writes linear float pixels straight into the 8-bit PNG, so sample it as linear
    palette.set_editor_property("srgb", False)
    roughness.set_editor_property("srgb", False)
    roughness.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    save(palette)
    save(roughness)
    path = DEST + "/Materials/M_DJG_Palette"
    mat = EAL.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        "M_DJG_Palette", DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    # Without the usage flag the engine substitutes the default material on skinned meshes in game.
    mat.set_editor_property("used_with_skeletal_mesh", True)
    lib.delete_all_material_expressions(mat)
    for texture, prop, y in [(palette, unreal.MaterialProperty.MP_BASE_COLOR, 0),
                             (roughness, unreal.MaterialProperty.MP_ROUGHNESS, 220)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -380, y)
        node.texture = texture
        node.sampler_type = (unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if texture == roughness
                             else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        lib.connect_material_property(node, "RGB" if texture == palette else "R", prop)
    lib.recompile_material(mat)
    save(mat)
    return mat


def import_meshes(manifest, mat):
    skeleton_path = DEST + "/Meshes/SK_DJG_Common_Skeleton"
    skeleton = EAL.load_asset(skeleton_path) if EAL.does_asset_exist(skeleton_path) else None
    meshes = {}
    for entry in manifest["characters"]:
        path = ROOT / entry["mesh_fbx"]
        mesh = import_asset(path, DEST + "/Meshes", fbx_options(skeleton), unreal.FbxFactory())
        if not isinstance(mesh, unreal.SkeletalMesh):
            raise TypeError("Expected SkeletalMesh: " + mesh.get_path_name())
        if skeleton is None:
            skeleton = mesh.skeleton
            if not EAL.rename_asset(skeleton.get_path_name(), skeleton_path):
                raise RuntimeError("Could not name common skeleton")
        if mesh.skeleton != skeleton:
            raise RuntimeError("Mesh did not share common skeleton: " + mesh.get_name())
        slots = list(mesh.materials)
        if len(slots) != 1:
            raise RuntimeError("Expected one material slot: " + mesh.get_name())
        slots[0].material_interface = mat
        mesh.materials = slots
        bounds = mesh.get_imported_bounds()
        height = bounds.box_extent.z * 2
        if not 140 < height < 260:
            raise RuntimeError("Unexpected mesh height in cm: " + str(height))
        save(mesh)
        meshes[entry["name"]] = mesh
        RESULT["meshes"].append({"asset": mesh.get_path_name(), "skeleton": skeleton.get_path_name(),
                                 "height_cm": height, "material_slots": len(slots)})
    save(skeleton)
    RESULT["skeleton"] = skeleton.get_path_name()
    return skeleton, meshes


def import_animations(manifest, skeleton):
    animations, montages = {}, {}
    for entry in manifest["animations"]:
        name = entry["name"]
        path = ROOT / "Exports/Animations" / (entry["asset"] + ".fbx")
        if not path.is_file():
            if name in REQUIRED:
                raise FileNotFoundError(path)
            RESULT["warnings"].append("Optional animation missing: " + name)
            continue
        animation = import_asset(path, DEST + "/Animations", fbx_options(skeleton, True), unreal.FbxFactory())
        if not isinstance(animation, unreal.AnimSequence) or animation.get_skeleton() != skeleton:
            raise RuntimeError("Invalid animation skeleton: " + name)
        animation.set_editor_property("enable_root_motion", False)
        animation.set_editor_property("force_root_lock", True)
        length = animation.get_play_length()
        if abs(length - entry["duration"]) > 1.0 / manifest["fps"]:
            raise RuntimeError("Animation duration mismatch: " + name + " " + str(length))
        save(animation)
        animations[name] = animation
        RESULT["animations"].append({"name": name, "asset": animation.get_path_name(), "duration": length,
                                     "contact_seconds": entry.get("contact_seconds")})
        if name not in {"Idle", "Walk", "Run", "SlideStart", "SlideLoop", "SlideRecover"}:
            montage_name = "AM_DJG_" + name
            montage_path = DEST + "/Montages/" + montage_name
            if EAL.does_asset_exist(montage_path):
                montage = EAL.load_asset(montage_path)
                if montage.get_first_anim_reference() != animation:
                    raise RuntimeError("Existing montage has different source: " + montage_path)
            else:
                factory = unreal.AnimMontageFactory()
                factory.target_skeleton = skeleton
                factory.source_animation = animation
                montage = TOOLS.create_asset(montage_name, DEST + "/Montages", unreal.AnimMontage, factory)
            contact = entry.get("contact_seconds")
            if contact is not None and name != "Slide":
                if hasattr(unreal, "JGRigAnimationLibrary"):
                    if not unreal.JGRigAnimationLibrary.configure_contact_montage(montage, contact):
                        raise RuntimeError("Could not configure contact section: " + name)
                else:
                    EAL.set_metadata_tag(montage, "DJG.ContactSeconds", str(contact))
                    RESULT["warnings"].append(name + ": Contact section deferred until native helper is compiled; timing saved in metadata.")
            save(montage)
            montages[name] = montage
            RESULT["montages"].append({"asset": montage.get_path_name(), "sections": [str(montage.get_section_name(i))
                for i in range(montage.get_num_sections())], "contact_seconds": contact})
    missing = REQUIRED - animations.keys()
    if missing:
        raise RuntimeError("Required animations absent from manifest: " + str(sorted(missing)))
    return montages


def create_data(meshes, montages):
    # Preserve GDD stats on the existing DA_Char_KimDoyun. Generated choices use neutral stats.
    for name, mesh in meshes.items():
        asset_name = "DA_DJG_" + name
        path = DEST + "/Data/" + asset_name
        if EAL.does_asset_exist(path):
            data = EAL.load_asset(path)
        else:
            # DataAssetFactory falls back to the requested class; its DataAssetClass is not exposed to Python.
            data = TOOLS.create_asset(asset_name, DEST + "/Data", unreal.JGCharacterData, unreal.DataAssetFactory())
            if not isinstance(data, unreal.JGCharacterData):
                raise RuntimeError("Could not create character data: " + asset_name)
            data.set_editor_property("character_id", "DJG_" + name)
            data.set_editor_property("display_name", name.replace("_", " "))
        data.set_editor_property("skeletal_mesh", mesh)
        if hasattr(unreal, "JGAnimInstance"):
            data.set_editor_property("anim_class", unreal.JGAnimInstance)
        for clip, field in MONTAGE_FIELDS.items():
            data.set_editor_property(field, montages[clip])
        save(data)
        RESULT["data_assets"].append(data.get_path_name())


def main():
    manifest = json.loads((ROOT / "Reports/rig_manifest.json").read_text(encoding="utf-8"))
    if not RESULT["runtime_integration_ready"]:
        RESULT["warnings"].append("Native animation classes are not compiled; imported content remains independent of the existing game character.")
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    for folder in ("Meshes", "Animations", "Montages", "Materials", "Textures", "Data"):
        EAL.make_directory(DEST + "/" + folder)
    mat = material()
    skeleton, meshes = import_meshes(manifest, mat)
    montages = import_animations(manifest, skeleton)
    create_data(meshes, montages)
    RESULT["success"] = True


try:
    main()
except Exception:
    RESULT["error"] = traceback.format_exc()
    raise
finally:
    REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DJG import report: " + str(REPORT))
