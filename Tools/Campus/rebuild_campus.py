"""Single commandlet entry: rebuild L_CampusEnclosed from the GLB, apply campus_edits.json, then copy it
into L_Campus_Match and L_Campus_Lobby (needs setup_lobby_assets.py to have run once).

Close the editor first (it holds the map), then from the project root:
UnrealEditor-Cmd.exe DoJokgu.uproject -run=pythonscript -script=<project>/Tools/Campus/rebuild_campus.py
  "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities" -unattended -nullrhi -nosound -DDC-ForceMemoryCache
Reports: Tools/Campus/Reports/build_report.json, cleanup_report.json, lobby_maps_report.json
"""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"))
import build_campus_level  # noqa: E402
import cleanup_campus_level  # noqa: E402
import build_lobby_maps  # noqa: E402

if not build_campus_level.run():
    unreal.log_error("CAMPUS_CLEANUP skipped: build failed")
elif not cleanup_campus_level.run():
    unreal.log_error("CAMPUS_LOBBY_MAPS skipped: cleanup failed")
else:
    build_lobby_maps.run()
