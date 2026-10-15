"""UE 5.8 Python commandlet: connect the five playable characters to the DJG rig and the roster asset.

The roster (/Game/Jokgu/Data/DA_JGCharacterRoster) is the one list the lobby, the match and the select screen
read; Project Settings > Jokgu > Characters points at it (DefaultGame.ini). List order = select screen order,
and the first two start as team A and B.

Mapping (2026-10-08): KimDoyun=02 RedAce, LeeMinji=03 Ponytail, ChoiTaesung=04 GreenPower,
GoYunseo=06 GlassesBun, HanJaemin=01 Captain. Stats stay 3 until the GDD table is filled in.
Existing KimDoyun stats are kept. Safe to run again.
"""
import json
import traceback
from pathlib import Path
import unreal

EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
RIG = "/Game/Jokgu/Characters/Rigged"
DATA = "/Game/Jokgu/Data/Characters"
REPORT = Path(unreal.Paths.project_dir()).resolve() / "Art/Characters/Rigged/Reports/roster_setup_report.json"

ROSTER = [
    ("KimDoyun", "김도윤", "02_RedAce"),
    ("LeeMinji", "이민지", "03_Ponytail"),
    ("ChoiTaesung", "최태성", "04_GreenPower"),
    ("GoYunseo", "고윤서", "06_GlassesBun"),
    ("HanJaemin", "한재민", "01_Captain"),
]
MONTAGES = {"tap_montage": "Tap", "weak_kick_montage": "WeakKick", "medium_kick_montage": "MediumKick",
            "strong_kick_montage": "StrongKick", "serve_montage": "Serve", "slide_montage": "Slide"}


def load(path):
    asset = EAL.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset " + path)
    return asset


def character_data(character_id):
    path = DATA + "/DA_Char_" + character_id
    if EAL.does_asset_exist(path):
        return load(path), False
    asset = TOOLS.create_asset("DA_Char_" + character_id, DATA, unreal.JGCharacterData, unreal.DataAssetFactory())
    if not asset:
        raise RuntimeError("Could not create " + path)
    return asset, True


def main():
    result = {"characters": [], "success": False}
    roster = []
    for character_id, display_name, look in ROSTER:
        data, created = character_data(character_id)
        data.set_editor_property("character_id", character_id)
        data.set_editor_property("display_name", display_name)
        data.set_editor_property("skeletal_mesh", load(RIG + "/Meshes/SK_DJG_" + look))
        data.set_editor_property("anim_class", unreal.JGAnimInstance)
        for field, montage in MONTAGES.items():
            data.set_editor_property(field, load(RIG + "/Montages/AM_DJG_" + montage))
        if created:
            for stat in ("move_speed_stat", "kick_power_stat", "accuracy_stat", "reach_stat"):
                data.set_editor_property(stat, 3)
        if not EAL.save_loaded_asset(data, only_if_is_dirty=False):
            raise RuntimeError("Could not save " + data.get_path_name())
        roster.append(data)
        result["characters"].append({"id": character_id, "name": display_name, "look": look, "created": created,
                                     "stats": [data.get_editor_property(s) for s in
                                               ("move_speed_stat", "kick_power_stat", "accuracy_stat", "reach_stat")]})

    roster_path = "/Game/Jokgu/Data/DA_JGCharacterRoster"
    if EAL.does_asset_exist(roster_path):
        roster_asset = load(roster_path)
    else:
        roster_asset = TOOLS.create_asset("DA_JGCharacterRoster", "/Game/Jokgu/Data", unreal.JGCharacterRoster,
                                          unreal.DataAssetFactory())
        if not roster_asset:
            raise RuntimeError("Could not create " + roster_path)
    roster_asset.set_editor_property("characters", roster)
    roster_asset.set_editor_property("default_character", roster[0])
    if not EAL.save_loaded_asset(roster_asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + roster_path)

    result["roster"] = [d.get_path_name() for d in roster_asset.get_editor_property("characters")]
    result["success"] = len(result["roster"]) == len(ROSTER)
    return result


try:
    output = main()
except Exception:
    output = {"success": False, "error": traceback.format_exc()}
REPORT.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("ROSTER_SETUP " + ("OK" if output.get("success") else "FAILED"))
