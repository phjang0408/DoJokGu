"""Read-only API probe for the installed Unreal version; writes a JSON report."""
import json
from pathlib import Path
import unreal

report = {"engine": unreal.SystemLibrary.get_engine_version(), "classes": {}}
names = ["AnimMontage", "CompositeSection", "AnimLinkableElement", "AnimBlueprintFactory",
         "AnimBlueprint", "BlueprintEditorLibrary", "AnimationBlueprintLibrary", "AnimSequence",
         "Skeleton", "SkeletalMesh", "FbxImportUI", "FbxSkeletalMeshImportData",
         "FbxAnimSequenceImportData", "EditorAssetLibrary", "AnimMontageFactory"]
for name in names:
    cls = getattr(unreal, name, None)
    report["classes"][name] = {"available": cls is not None, "members": dir(cls) if cls else []}
    if name in ("CompositeSection", "AnimLinkableElement") and cls:
        report["classes"][name]["documentation"] = cls.__doc__
for name in ("FbxImportUI", "FbxSkeletalMeshImportData", "FbxAnimSequenceImportData"):
    report["classes"][name]["documentation"] = getattr(unreal, name).__doc__
report["assets"] = list(unreal.EditorAssetLibrary.list_assets("/Game/Jokgu", recursive=True))
output = Path(unreal.Paths.project_dir()) / "Art/Characters/Rigged/Unreal/api_probe.json"
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("DJG API probe saved: " + str(output))
