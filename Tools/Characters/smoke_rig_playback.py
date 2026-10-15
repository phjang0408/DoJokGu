"""Exercise the native animation instance in a temporary editor actor; never save the map."""
import unreal as u,json,time,traceback
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()/'Art/Characters/Rigged'
report={'test':'native animation instance in PIE simulation','phases':[],'failures':[],'complete':False}
actor=None;preview_actor=None;handle=None;phase=-1;started=0.;samples=[]
base='/Game/Jokgu/Characters/Rigged'
manifest=json.loads((root/'Reports/rig_manifest.json').read_text())
phases=[('Idle',0),('Walk',80),('Run',450)]+[(e['name'],0) for e in manifest['animations'] if e['name'] not in ['Idle','Walk','Run','SlideStart','SlideLoop','SlideRecover']]
def flush():
    (root/'Reports/playback_smoke.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def finish():
    global handle
    if handle:u.unregister_slate_post_tick_callback(handle);handle=None
    if actor:actor.destroy_actor()
    if preview_actor:u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(preview_actor)
    u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_end_play()
    flush();u.SystemLibrary.quit_editor()
def begin():
    global phase,started,samples
    phase+=1;samples=[];started=time.monotonic()
    if phase>=len(phases):
        report['complete']=True;finish();return
    name,speed=phases[phase];movement.set_editor_property('velocity',u.Vector(speed,0,0))
    anim.montage_stop(0.)
    if name not in ['Idle','Walk','Run']:
        m=u.load_asset(base+'/Montages/AM_DJG_'+name);duration=anim.montage_play(m,1.)
        if duration<=0:report['failures'].append(name+': montage rejected')
        if any(str(m.get_section_name(i))=='Contact' for i in range(m.get_num_sections())):anim.montage_jump_to_section('Contact',m)
    flush()
def tick(dt):
    try:
        if actor is None:
            setup()
            return
        name,speed=phases[phase]
        movement.set_editor_property('velocity',u.Vector(speed,0,0))
        if time.monotonic()-started<.15:return
        row=[]
        for bone in ['root','foot_l','foot_r','hand_l','hand_r']:
            v=mesh.get_socket_transform(bone,u.RelativeTransformSpace.RTS_COMPONENT).translation;row.extend([v.x,v.y,v.z])
        m=anim.get_current_active_montage();row.append(anim.montage_get_position(m) if m else -1)
        row.append(actor.get_velocity().length())
        samples.append(row)
        if time.monotonic()-started>=1.65:
            delta=max(max(s[i] for s in samples)-min(s[i] for s in samples) for i in range(3,15))
            root_delta=max(max(s[i] for s in samples)-min(s[i] for s in samples) for i in range(3))
            report['phases'].append({'name':name,'ticks':len(samples),'bone_motion_cm':delta,'root_motion_cm':root_delta,'montage_positions':[samples[0][15],max(s[15] for s in samples)],'speed_cm_s':[min(s[16] for s in samples),max(s[16] for s in samples)]})
            if delta<(.05 if name not in ['Walk','Run'] else 3.):report['failures'].append(name+': insufficient evaluated bone movement')
            if root_delta>.1:report['failures'].append(name+': root moved')
            begin()
    except Exception:report['failures'].append(traceback.format_exc());finish()
def setup():
    global actor,mesh,movement,anim
    for world in u.EditorLevelLibrary.get_pie_worlds(True):
        actors=u.GameplayStatics.get_all_actors_of_class(world,u.JGCharacter)
        actor=next((a for a in actors if a.get_actor_label()=='DJG_RigPlaybackTest' and a.has_authority()),None)
        if actor:break
    if actor is None:
        if time.monotonic()-boot_time>45:raise RuntimeError('Authoritative PIE copy did not start within 45 seconds')
        return
    actor.set_actor_tick_enabled(False)
    actor.set_actor_enable_collision(False)
    report['authority']=actor.has_authority()
    mesh=actor.get_component_by_class(u.SkeletalMeshComponent);movement=actor.get_component_by_class(u.CharacterMovementComponent)
    movement.set_component_tick_enabled(False)
    mesh.set_skeletal_mesh_asset(u.load_asset(base+'/Meshes/SK_DJG_01_Captain'))
    mesh.set_anim_instance_class(u.JGAnimInstance);mesh.set_update_animation_in_editor(True)
    mesh.set_editor_property('visibility_based_anim_tick_option',u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    anim=mesh.get_anim_instance()
    if not anim:raise RuntimeError('Native anim instance missing')
    report['anim_instance']=anim.get_class().get_path_name();begin()

try:
    boot_time=time.monotonic()
    preview_actor=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.JGCharacter,u.Vector(0,0,300))
    preview_actor.set_actor_label('DJG_RigPlaybackTest')
    u.get_editor_subsystem(u.LevelEditorSubsystem).editor_play_simulate()
    handle=u.register_slate_post_tick_callback(tick)
except Exception:report['failures'].append(traceback.format_exc());finish()
