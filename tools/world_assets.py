"""Private original-disc asset conversion and tuning for the larger test map."""
import re
import struct
import zlib
from game_archive import rarc
from j3d_model import Block,names,bti,tiled_rgba,convert,brk_final_colors
import j3d_animation

PATHS=['enemy/data/Kochappy/model.szs','enemy/data/Pelplant/model.szs','enemy/data/Hiba/model.szs',
       'enemy/data/ElecHiba/model.szs','user/Kando/objects/pikihead/arc.szs','user/Kando/map/waterbox.szs',
       'user/Abe/Pellet/pal/pellet.szs','user/Abe/Pellet/pal/pelletlist_pal.szs','user/Abe/time/time.ini',
       'enemy/data/YellowChappy/moyou_565.2.bti','enemy/data/YellowChappy/swallow_565.2.bti',
       'user/Abe/Pellet/pal/bolt.szs','user/Abe/Pellet/pal/apple_blue.szs',
       'user/Mukki/mapunits/arc/room_block1_3_hiba_tsuchi/arc.szs',
       'enemy/data/Chappy/anim.szs','enemy/data/Kochappy/anim.szs']


def config(data,name):
    records=[r for r in re.findall(rb'\{([^{}]+)\}',data) if re.search(rb'\bname\s+'+name.encode()+rb'\s',r)]
    if len(records)!=1:raise ValueError('Missing original configuration: '+name)
    return {m[0].decode():float(m[1]) for m in re.findall(rb'(?m)^\s*([a-z_]+)\s+([0-9.]+)',records[0])}


def build(resources,pikis,romfs,source,report,parameter):
    models=report['models']
    def save(name,data,**kwargs):
        result,info=convert(data,**kwargs);(romfs/(name+'.p2m')).write_bytes(result);models[name]=info
    # These are the original Piki::pikiColors, not an invented palette.
    colors=[(0,50,255),(255,30,0),(255,210,0)]
    for label,color in [('purple',(130,0,170)),('white',(255,255,255))]:
        save(label,pikis['piki_model/piki_p2_'+('black' if label=='purple' else 'white')+'.bmd'],tint=color,tint_materials=(0,),skinned=True)
    for color,label in enumerate(('blue','red','yellow')):
        if label!='red':save(label,pikis['piki_model/piki_p2_'+label+'.bmd'],tint=colors[color],tint_materials=(0,),skinned=True)
        seed=rarc(resources[PATHS[4]])['pikihead.bmd'];save('seed_'+label,seed,tint=colors[color],tint_materials=(0,))
        onion=rarc(resources['user/Kando/onyon/arc.szs'])
        if label!='red':save('onion_'+label,onion['goal.bmd'],material_colors=brk_final_colors(onion['kidou_'+label+'.brk']),bake_materials=True)
        effect=rarc(resources['user/Kando/effect/modeleffect.szs'])
        if label!='red':save('spot_'+label,effect['onyonspot.bmd'],material_colors=brk_final_colors(effect['onyonspot_'+label+'.brk']),unlit=True)
    for label,path in [('dwarf',PATHS[0]),('posy',PATHS[1]),('fire',PATHS[2]),('electric',PATHS[3]),('water',PATHS[5])]:
        arc=rarc(resources[path]);entries=[v for k,v in arc.items() if k.endswith('.bmd')]
        if len(entries)!=1:raise ValueError('Ambiguous original model: '+path)
        save(label,entries[0],unlit=label in ('water','posy'),skinned=label=='dwarf',bake_materials=label=='posy')
    red=rarc(resources['enemy/data/Chappy/model.szs'])['enemy.bmd']
    for label in ('cursor','marker'):
        save(label,pikis['cursor/'+label+'.bmd'],material_colors=brk_final_colors(pikis['cursor/'+('arrow_orima' if label=='cursor' else 'wakka_orima')+'.brk']),unlit=True)
    save('orange',red,external_textures={0:resources[PATHS[9]],1:resources[PATHS[10]]},skinned=True)
    report['animations']={}
    def animation(name,model,arc,clips):
        data,info=j3d_animation.convert(model,[(state,label,arc[label]) for state,label in clips])
        (romfs/(name+'.p2a')).write_bytes(data);report['animations'][name]=info
    common=[(0,'motion/wait.bca'),(1,'motion/walk.bca'),(2,'motion/run2.bca'),
            (3,'motion/attack1.bca'),(4,'motion/pickloop.bca'),(5,'motion/rolljmp.bca'),
            (6,'motion/hang.bca'),(7,'motion/fue.bca'),(8,'motion/nuku.bca'),
            (9,'motion/oboreru.bca'),(10,'motion/dead.bca')]
    for name,path in [('olimar','orima_model/orima1.bmd'),('louie','orima_model/orima3.bmd')]+[(c,'piki_model/piki_p2_'+c+'.bmd') for c in ('blue','red','yellow','black','white')]:
        clips=common
        if name in ('olimar','louie'):
            replacements={3:'motion/punch.bca',5:'motion/throw.bca',6:'motion/trwwait.bca'}
            clips=[(state,replacements.get(state,label)) for state,label in common]
        animation('purple' if name=='black' else name,pikis[path],pikis,clips+[(11,'motion/nigeru.bca'),(12,'motion/kizuku.bca'),(13,'motion/gattu.bca'),(14,'motion/nukareru.bca'),(15,'motion/gdead.bca'),(16,'motion/hnoboru.bca')])
    for name,model,arc in [('bulborb',red,rarc(resources[PATHS[14]])),('orange',red,rarc(resources[PATHS[14]])),('dwarf',rarc(resources[PATHS[0]])['enemy.bmd'],rarc(resources[PATHS[15]]))]:
        wait='type1.bca' if name!='dwarf' else 'wait1.bca'
        animation(name,model,arc,[(0,wait),(1,'move1.bca'),(3,'attack.bca'),(10,'dead.bca')])
    pellet=rarc(resources[PATHS[6]])
    # Original PelletNumber::Object::changeMaterial, register bpel1.
    pellet_colors=[(0,51,255,255),(251,17,0,255),(255,220,52,255)]
    for value in range(4):
        for color in range(3):save(f'pellet_{value}_{color}',pellet[f'white{value+1}.bmd'],material_colors={'bpel1':pellet_colors[color]},bake_materials=True,unlit=True)
    treasure_configs=resources['user/Abe/Pellet/pal/otakara_config.txt'];treasures=[]
    for label,path in [('bolt',PATHS[11]),('apple_blue',PATHS[12])]:
        entry=config(treasure_configs,label);arc=rarc(resources[path]);entries=[v for k,v in arc.items() if k.endswith('.bmd')]
        if len(entries)!=1:raise ValueError('Ambiguous treasure model: '+label)
        save(label,entries[0])
    for label in ('ahiru','bolt','apple_blue'):treasures.append(config(treasure_configs,label))
    enemy_archive=rarc(resources['enemy/parm/enemyParms.szs']);carcasses=resources['user/Abe/Pellet/pal/carcass_config.txt']
    enemies=[]
    for folder,label in [('chappy','Chappy'),('yellowchappy','YellowChappy'),('kochappy','Kochappy')]:
        data=enemy_archive[folder+'/enemyparm.txt'];groups=re.findall(rb'(?m)^\s*\{\s*\r?\n(.*?)^\s*\}',data,re.S|re.M)
        general=next((g for g in groups if b'{fp24}' in g),None)
        if general is None:raise ValueError('Missing enemy tuning '+folder)
        c=config(carcasses,label)
        enemies.append([parameter(general,k) for k in ('fp00','fp06','fp11','fp22','fp24')]+[c[k] for k in ('min','max','pikicountmin','radius','height')])
    number_config=rarc(resources[PATHS[7]])['numberpellet_config.txt'];pellet_config=[]
    for value in (1,5,10,20):
        c=config(number_config,'number'+str(value));pellet_config.append([c[k] for k in ('min','max','pikicountmax','pikicountmin','radius','height')])
    piki=resources['user/Abe/piki/pikiParms.txt'];navi=resources['user/Abe/piki/naviParms.txt'];time=resources[PATHS[8]]
    real_day=parameter(time,'fp02')*(parameter(time,'fp01')-parameter(time,'fp00'))/24
    lines=['/* Private tuning converted from this user disc; generated by builder. */']
    def cf(value):return repr(float(value))+'f'
    def table(name,values):lines.append('static const float '+name+f'[{len(values)}][{len(values[0])}]={{'+','.join('{'+','.join(cf(v) for v in row)+'}' for row in values)+'};')
    table('demo_enemy_tuning',enemies)
    table('demo_treasure_tuning',[[c[k] for k in ('min','max','money','radius','height')] for c in treasures])
    table('demo_pellet_tuning',pellet_config)
    lines.extend(['#define DEMO_DAY_SECONDS '+cf(real_day),'#define DEMO_FIRE_TIME '+cf(parameter(piki,'p055'))])
    # Original Navi throw yellow parameter id is resolved from its declaration.
    lines.append('#define DEMO_YELLOW_THROW '+cf(parameter(navi,'p054')))
    lines.append('#define DEMO_WHITE_SPEED '+cf(parameter(piki,'P001')))
    lines.append('#define DEMO_PURPLE_SPEED '+cf(parameter(piki,'P002')))
    lines.append('static const float demo_piki_scale[5]={'+','.join(cf(parameter(piki,k)) for k in ('p007','p007','p007','p009','p008'))+'};')
    lines.append('static const float demo_piki_damage[5]={'+','.join(cf(parameter(piki,k)) for k in ('p004','p012','p013','P003','P004'))+'};')
    (source/'demo_settings.h').write_text('\n'.join(lines)+'\n',encoding='ascii')
    report['world_tuning']={'enemy':enemies,'treasures':treasures,'pellets':pellet_config,'day_seconds':real_day}
    # Genuine dirt texture from a cave-room model, on a newly authored flat mesh.
    terrain=rarc(resources[PATHS[13]])['view.bmd'];d=Block(terrain);offset=32;tex=None
    for _ in range(d.u32(12)):
        tag,size=d.read('4sI',offset)
        if tag==b'TEX1':tex=Block(d.get(offset,size));break
        offset+=size
    if tex is None:raise ValueError('Original ground texture missing')
    label='tsuchi_floor_256x256_s3tc';idx=names(tex,tex.u32(16)).index(label);h=tex.u32(12)+idx*32
    w,he,pixels=bti(tex.data[h:])
    # The original atlas's lower half is dirt; its upper half is a wall.
    # Box prefilter the floor region to avoid severe minification shimmer.
    crop=bytearray()
    for y in range(32):
        for x in range(64):
            for c in range(4):crop.append(sum(pixels[((he//2+y*4+dy)*w+x*4+dx)*4+c] for dy in range(4) for dx in range(4))//16)
    w,he,pixels=64,32,crop;native=tiled_rgba(pixels,w,he)
    payload=bytearray(struct.pack('<4I',w,he,1,1)+native+struct.pack('<iII',0,6,1))
    for x,z,u,v in [(-1,-1,0,0),(1,-1,8,0),(1,1,8,8),(-1,-1,0,0),(1,1,8,8),(-1,1,0,8)]:
        payload.extend(struct.pack('<12f',x,0,z,1,1,1,1,1,u,1-v,0,0))
    (romfs/'terrain.p2m').write_bytes(struct.pack('<4s5I',b'P2M2',len(payload),zlib.crc32(payload),1,1,6)+payload)
    report['ground_texture']={'source':PATHS[13]+'/view.bmd','texture':label}
