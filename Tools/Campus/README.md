# Campus 맵 빌드·정리 도구

`L_CampusEnclosed`를 GLB에서 다시 만들고 정리 편집을 적용한다. 맵과 에셋(`Content/`), 원본(`Art/`)은 git에 없으므로 이 폴더의 스크립트와 편집 목록이 재현 기준이다. 설계는 `docs/superpowers/specs/2026-10-08-campus-lobby-design.md` 4절.

| 파일 | 역할 |
| --- | --- |
| `build_campus_level.py` | 빈 맵 + 조명·코트·GameMode, GLB 장면 가져오기, 코트 선·네트 맞춤, 배경 충돌 제거, 관중 배치 |
| `cleanup_campus_level.py` | `campus_edits.json` 적용: 삭제, 이동·회전·크기, 소품 추가, 담장 안 충돌 |
| `rebuild_campus.py` | 위 두 단계를 차례로 실행하는 commandlet 진입점 |
| `analyze_campus.py` | Blender에서 GLB를 불러와 안 보이는 객체와 2.5m 미만 나무 쌍을 찾는다. 편집하지 않고 보고서만 쓴다 |
| `campus_edits.json` | 정리 편집 목록. 라벨 = GLB 노드 이름의 `.`을 `_`로 바꾼 것 |
| `Reports/` | `analysis_report.json`, `build_report.json`, `cleanup_report.json` |

> 주의: `campus_edits.json`은 10/8 16:02 export GLB 기준이다(개별 편집은 비어 있고 충돌 목록만 있다). Blender에서 물체를 추가·삭제하고 다시 export하면 라벨 번호가 밀려 개별 편집이 다른 물체에 적용될 수 있다. 다시 export한 뒤에는 분석부터 다시 하고 목록을 갱신한다(설계 문서 4.1절).

좌표: Blender 미터 (bx, by, bz) = Unreal 센티미터 (100·by, 100·bx, 100·bz).

## 실행

GLB 내보내기 (Blender 원본을 저장한 뒤, 명령줄에서). Blender Scripting 탭의 Run Script로 실행하면 `__file__`이 `<blend 경로>\<스크립트 이름>`이 되어 출력 위치가 `CampusEnclosed/CampusEnclosed`로 잘못 잡히므로 쓰지 않는다:

```
"D:/New Folder/15_Blender/blender.exe" -b "D:/01_Github/DoJokgu/Art/Schoolyard/CampusEnclosed/DJG_CampusEnclosed.blend" --python "D:/01_Github/DoJokgu/Art/Schoolyard/Scripts/export_campus_enclosed.py"
```

분석 (에디터와 무관, 약 11분):

```
"D:\New Folder\15_Blender\blender.exe" -b --python Tools/Campus/analyze_campus.py -- Art/Schoolyard/CampusEnclosed/Exports/DJG_CampusEnclosed.glb
```

맵 다시 만들기 (에디터를 닫고, Bash에서):

```
"D:/03_UEProgram/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/01_Github/DoJokgu/DoJokgu.uproject" \
  -run=pythonscript -script="D:/01_Github/DoJokgu/Tools/Campus/rebuild_campus.py" \
  "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities" -unattended -nullrhi -nosound -DDC-ForceMemoryCache
```

성공 여부는 종료 코드가 아니라 두 보고서의 `success`로 확인한다.
