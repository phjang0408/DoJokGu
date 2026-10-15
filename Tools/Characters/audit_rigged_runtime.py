"""Read-only audit of imported rig assets and current game defaults."""
import json,traceback
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()/'Art/Characters/Rigged'
out={'engine':u.SystemLibrary.get_engine_version(),'assets':{},'defaults':{},'api':{},'errors':[]}
def path(v):return v.get_path_name() if v else None
def props(obj,names):
    values={}
    for n in names:
        try:
            v=obj.get_editor_property(n)
            values[n]=path(v) if isinstance(v,u.Object) else str(v)
        except Exception as e:values[n]='ERROR: '+str(e)
    return values
try:
    for name in ['JGAnimInstance','JGRigAnimationLibrary','AnimationBlueprintLibrary','AnimPoseExtensions','AnimPoseEvaluationOptions']:
        cls=getattr(u,name,None);out['api'][name]=[m for m in dir(cls) if any(w in m for w in ['pose','bone','evaluate','montage','tick'])] if cls else None
    manifest=json.loads((root/'Reports/rig_manifest.json').read_text())
    base='/Game/Jokgu/Characters/Rigged'
    for e in manifest['characters']:
        mesh=u.load_asset(base+'/Meshes/SK_DJG_'+e['name']);data=u.load_asset(base+'/Data/DA_DJG_'+e['name'])
        out['assets'][e['name']]={'mesh':path(mesh),'skeleton':path(mesh.skeleton) if mesh else None,'data':props(data,['character_id','anim_class','skeletal_mesh']) if data else None}
    for e in manifest['animations']:
        a=u.load_asset(base+'/Animations/'+e['asset']);row={'sequence':path(a),'duration':a.get_play_length() if a else None}
        if e.get('contact_seconds') is not None:
            m=u.load_asset(base+'/Montages/AM_DJG_'+e['name']);row['sections']=[str(m.get_section_name(i)) for i in range(m.get_num_sections())];row['contact_start']=m.get_section_start_time(m.get_section_index('Contact')) if hasattr(m,'get_section_start_time') else None
        out['assets'][e['name']]=row
    for p in ['/Game/Jokgu/Core/BP_JGGameMode','/Game/Jokgu/Core/BP_JGGameMode_Practice','/Game/Jokgu/Characters/BP_JGCharacter']:
        cls=u.EditorAssetLibrary.load_blueprint_class(p);obj=u.get_default_object(cls)
        out['defaults'][p]={} if 'GameMode' in p else props(obj,['character_data'])
        if 'Character' in p:
            mesh=obj.get_component_by_class(u.SkeletalMeshComponent)
            out['defaults'][p]['mesh']=props(mesh,['skeletal_mesh_asset','anim_class','relative_location','relative_rotation','relative_scale3d'])
    roster=u.load_asset('/Game/Jokgu/Data/DA_JGCharacterRoster')
    out['defaults']['Roster']=props(roster,['default_character','characters']) if roster else None
    data=u.load_asset('/Game/Jokgu/Data/Characters/DA_Char_KimDoyun')
    out['defaults']['KimDoyun']=props(data,['character_id','skeletal_mesh','anim_class','tap_montage','weak_kick_montage','medium_kick_montage','strong_kick_montage','serve_montage','slide_montage'])
except Exception:out['errors'].append(traceback.format_exc())
(root/'Reports/runtime_audit.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
if out['errors']:raise RuntimeError(out['errors'])
u.log('DJG_RUNTIME_AUDIT_PASS')
