"""UE 5.8 Python commandlet (2026-10-08 tuning).

- T_DJG_Palette holds linear values in 8-bit (written straight from Blender float pixels), so it must be
  sampled as linear. Reading it as sRGB darkened every colour, most visibly the skin.
- Camera: lower and further back so the background shows and high balls stay on screen
  (pitch -42, distance 2600, focus on the net; checked for 4.5 m apex on 19.5:9 and 16:9).
"""
import json
from pathlib import Path
import unreal

EAL = unreal.EditorAssetLibrary
REPORT = Path(unreal.Paths.project_dir()).resolve() / "Art/Characters/Rigged/Reports/visual_tuning_report.json"
result = {}

palette = EAL.load_asset("/Game/Jokgu/Characters/Rigged/Textures/T_DJG_Palette")
palette.set_editor_property("srgb", False)
result["palette_srgb"] = palette.get_editor_property("srgb")
EAL.save_loaded_asset(palette, only_if_is_dirty=False)

balance = EAL.load_asset("/Game/Jokgu/Data/DA_Balance_Default")
for key, value in {"camera_pitch": -42.0, "camera_distance": 2600.0, "camera_focus_distance_from_net": 0.0}.items():
    balance.set_editor_property(key, value)
    result[key] = balance.get_editor_property(key)
EAL.save_loaded_asset(balance, only_if_is_dirty=False)

REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("VISUAL_TUNING " + json.dumps(result))
