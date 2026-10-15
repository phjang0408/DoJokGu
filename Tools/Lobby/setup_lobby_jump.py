"""UE 5.8 Python commandlet (2026-10-15): lobby jump input. Run after building the AJGLobbyCharacter::JumpAction change.

- IA_Jump: copy of IA_DebugTap (a bool action, started on press).
- IMC_Lobby: copy of IMC_Match with the match-only actions (slide, attack tap/drag, rematch) removed; the attack keys
  (J, gamepad bottom face button) and Space now jump. Moving is unchanged.
- BP_JGLobbyPlayerController uses IMC_Lobby; BP_JGLobbyCharacter.JumpAction = IA_Jump.
Writes Tools/Lobby/Reports/lobby_jump_report.json.
"""
import json
import traceback
from pathlib import Path
import unreal

EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = Path(unreal.Paths.project_dir()).resolve() / "Tools/Lobby/Reports/lobby_jump_report.json"
ACTIONS = "/Game/Jokgu/Input/Actions"
RESULT = {"success": False}
MATCH_ONLY = ["IA_Slide", "IA_DebugTap", "IA_DebugDrag", "IA_Rematch"]
JUMP_KEYS = ["SpaceBar", "J", "Gamepad_FaceButton_Bottom"]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def copy(source, folder, name):
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        return EAL.load_asset(path)
    asset = TOOLS.duplicate_asset(name, folder, EAL.load_asset(source))
    require(asset, "Could not copy " + source + " to " + path)
    return asset


def key(name):
    k = unreal.Key()
    k.import_text(name)
    require(str(k.export_text()) == name, "Unknown key " + name)
    return k


def default_object(blueprint_path):
    blueprint = EAL.load_asset(blueprint_path)
    require(blueprint, "Missing " + blueprint_path)
    return blueprint, unreal.get_default_object(blueprint.generated_class())


def main():
    jump = copy(ACTIONS + "/IA_DebugTap", ACTIONS, "IA_Jump")
    require(str(jump.get_editor_property("value_type")) == str(unreal.InputActionValueType.BOOLEAN), "IA_Jump is not bool")
    require(EAL.save_loaded_asset(jump, False), "Could not save IA_Jump")

    imc = copy("/Game/Jokgu/Input/IMC_Match", "/Game/Jokgu/Input", "IMC_Lobby")
    for name in MATCH_ONLY + ["IA_Jump"]:
        imc.unmap_all_keys_from_action(EAL.load_asset(ACTIONS + "/" + name))
    for name in JUMP_KEYS:
        imc.map_key(jump, key(name))
    require(EAL.save_loaded_asset(imc, False), "Could not save IMC_Lobby")
    mappings = imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
    RESULT["imc_lobby"] = sorted(str(m.get_editor_property("action").get_name()) + ":" + str(m.get_editor_property("key").export_text())
                                 for m in mappings)
    require(not any(n.split(":")[0] in MATCH_ONLY for n in RESULT["imc_lobby"]), "IMC_Lobby still maps match actions")

    blueprint, controller = default_object("/Game/Jokgu/Lobby/BP_JGLobbyPlayerController")
    controller.set_editor_property("default_mapping_contexts", [imc])
    require(EAL.save_loaded_asset(blueprint, False), "Could not save BP_JGLobbyPlayerController")

    blueprint, character = default_object("/Game/Jokgu/Lobby/BP_JGLobbyCharacter")
    character.set_editor_property("jump_action", jump)
    require(EAL.save_loaded_asset(blueprint, False), "Could not save BP_JGLobbyCharacter")
    RESULT["controller_contexts"] = [c.get_name() for c in controller.get_editor_property("default_mapping_contexts")]
    RESULT["character_jump_action"] = character.get_editor_property("jump_action").get_name()
    RESULT["success"] = True


try:
    main()
except Exception:
    RESULT["error"] = traceback.format_exc()
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(RESULT, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("LOBBY_JUMP " + ("OK" if RESULT["success"] else "FAILED"))
