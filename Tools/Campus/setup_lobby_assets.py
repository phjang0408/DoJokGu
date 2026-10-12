"""UE 5.8 Python commandlet: assets the lobby and the settings panel need. Safe to run again.

- Audio: SC_Master with children SC_Effects and SC_Music, and SMix_Volume (the settings panel overrides
  the three classes through this mix). Project Settings > Jokgu > Audio points at them (DefaultGame.ini).
- Lobby blueprints: BP_JGLobbyCharacter (rig mesh, IA_Move), BP_JGLobbyPlayerController (IMC_Match),
  BP_JGLobbyGameMode (both above, the BP_JGGameMode roster).
Writes Tools/Campus/Reports/lobby_assets_report.json.
"""
import json
import traceback
from pathlib import Path
import unreal

EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus/Reports/lobby_assets_report.json"
AUDIO = "/Game/Jokgu/Audio"
CORE = "/Game/Jokgu/Core"
LOBBY = "/Game/Jokgu/Lobby"
RESULT = {"success": False, "created": [], "updated": []}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load(path):
    asset = EAL.load_asset(path)
    require(asset, "Missing asset " + path)
    return asset


def get_or_create(folder, name, asset_class, factory):
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        RESULT["updated"].append(path)
        return load(path)
    asset = TOOLS.create_asset(name, folder, asset_class, factory)
    require(asset, "Could not create " + path)
    RESULT["created"].append(path)
    return asset


def save(asset):
    require(EAL.save_loaded_asset(asset, False), "Could not save " + asset.get_path_name())


def setup_audio():
    master = get_or_create(AUDIO, "SC_Master", unreal.SoundClass, unreal.SoundClassFactory())
    effects = get_or_create(AUDIO, "SC_Effects", unreal.SoundClass, unreal.SoundClassFactory())
    music = get_or_create(AUDIO, "SC_Music", unreal.SoundClass, unreal.SoundClassFactory())
    # the master override reaches the children only through this parent link
    for child in (effects, music):
        child.set_editor_property("parent_class", master)
    master.set_editor_property("child_classes", [effects, music])
    mix = get_or_create(AUDIO, "SMix_Volume", unreal.SoundMix, unreal.SoundMixFactory())
    for asset in (master, effects, music, mix):
        save(asset)


def blueprint(name, parent_class):
    path = LOBBY + "/" + name
    if EAL.does_asset_exist(path):
        RESULT["updated"].append(path)
        bp = load(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = TOOLS.create_asset(name, LOBBY, unreal.Blueprint, factory)
        require(bp, "Could not create " + path)
        RESULT["created"].append(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, unreal.get_default_object(bp.generated_class())


def setup_lobby():
    match_mode = unreal.get_default_object(load(CORE + "/BP_JGGameMode").generated_class())

    character_bp, character = blueprint("BP_JGLobbyCharacter", unreal.JGLobbyCharacter)
    mesh = character.get_editor_property("mesh")
    mesh.set_editor_property("skeletal_mesh_asset", load("/Game/Jokgu/Characters/Rigged/Meshes/SK_DJG_01_Captain"))
    mesh.set_editor_property("anim_class", unreal.JGAnimInstance)
    character.set_editor_property("move_action", load("/Game/Jokgu/Input/Actions/IA_Move"))

    controller_bp, controller = blueprint("BP_JGLobbyPlayerController", unreal.JGLobbyPlayerController)
    controller.set_editor_property("default_mapping_contexts", [load("/Game/Jokgu/Input/IMC_Match")])

    mode_bp, mode = blueprint("BP_JGLobbyGameMode", unreal.JGLobbyGameMode)
    mode.set_editor_property("default_pawn_class", character_bp.generated_class())
    mode.set_editor_property("player_controller_class", controller_bp.generated_class())
    mode.set_editor_property("character_roster", match_mode.get_editor_property("character_roster"))
    mode.set_editor_property("default_character_data", match_mode.get_editor_property("default_character_data"))

    for bp in (character_bp, controller_bp, mode_bp):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
    RESULT["lobby_roster"] = len(mode.get_editor_property("character_roster"))


def main():
    setup_audio()
    setup_lobby()
    RESULT["success"] = True


def run():
    try:
        main()
    except Exception:
        RESULT["error"] = traceback.format_exc()
    REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("LOBBY_ASSETS " + ("OK" if RESULT["success"] else "FAILED"))
    return RESULT["success"]


if __name__ == "__main__":
    run()
