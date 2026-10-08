"""Single commandlet entry: rebuild L_CampusEnclosed from the GLB, then apply campus_edits.json.

Close the editor first (it holds the map), then from the project root:
UnrealEditor-Cmd.exe DoJokgu.uproject -run=pythonscript -script=<project>/Tools/Campus/rebuild_campus.py
  "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities" -unattended -nullrhi -nosound -DDC-ForceMemoryCache
Reports: Tools/Campus/Reports/build_report.json, cleanup_report.json
"""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_dir()).resolve() / "Tools/Campus"))
import build_campus_level  # noqa: E402
import cleanup_campus_level  # noqa: E402

if build_campus_level.run():
    cleanup_campus_level.run()
else:
    unreal.log_error("CAMPUS_CLEANUP skipped: build failed")
