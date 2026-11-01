"""Original title scene, logo coordinates, BCK motions and J2D menu art.

References: ebiP2Title{,Unit,Camera,CoordMgr,Pikmin}, ebiScreenTitleMenu,
projectPiki 999140f (CC0). Rendering/navigation/arrival glue remain native.
"""
import re
import struct
import zlib
from game_archive import rarc
from j3d_model import convert,bti
from j3d_animation import convert as animation
from j3d_key import KeyAnimation
from blo_layout import read_layout
PATHS=['user/Ebisawa/title/title.szs','user/Ebisawa/title/bg_summer.szs','new_screen/eng/title.szs']
GROUPS=['Ngame','Nvs','Nchallen','Noption','Nhiscore','Nomake']
STARTS=[100,295,495,695,895,1095]


def build_ui(arc,image,path):
    key=KeyAnimation(arc['anim/title_menu_6.bck']);frames=[]
    for f in range(120):
        pics,_=read_layout(arc['scrn/title_menu_6.blo'],key,100,dict(zip(GROUPS,[v+f for v in STARTS])))
        frames.append(pics)
    count=len(frames[0]);records=[]
    for i,p in enumerate(frames[0]):
        group=GROUPS.index(p['group']) if p['group'] in GROUPS else 7
        records.append((p,group,[row[i]['points'] for row in frames]))
    key=KeyAnimation(arc['anim/push_start_a.bck'])
    push=[read_layout(arc['scrn/push_start_a.blo'],key,f%key.frames)[0] for f in range(120)]
    for i,p in enumerate(push[0]):records.append((p,6,[row[i]['points'] for row in push]))
    copyright,_=read_layout(arc['scrn/tm_2003nintendo.blo'])
    for p in copyright:records.append((p,7,[p['points']]*120))
    payload=bytearray()
    for p,group,poses in records:
        idx=image('title_'+p['texture'],p['black'],p['white'],raw=bti(arc['timg/'+p['texture']]))
        flags=1 if p['tag'].startswith('Ppi') else 0
        colors=[v*p['material_color'][c]/65025 for corner in p['colors'] for c,v in enumerate(corner)]
        payload.extend(struct.pack('<3I24f',idx,group,flags,*colors,*p['uv']))
        payload.extend(struct.pack('<960f',*(v for points in poses for point in points for v in point)))
    path.write_bytes(struct.pack('<4s4I',b'P2T1',len(payload),zlib.crc32(payload),len(records),120)+payload)
    return {'panes':len(records),'menu_animation':'title_menu_6.bck: original per-category frame ranges','push_animation':'push_start_a.bck','subset':'BLO pictures and BCK transforms; BPK colours and complete E2D callbacks remain incomplete'}


def build(resources,romfs,source,report):
    arc=rarc(resources[PATHS[0]]);bg=rarc(resources[PATHS[1]])
    wait=KeyAnimation(bg['opening_wait.bck'])
    mesh,info=convert(bg['opening.bmd'],max_texture_size=128,joint_poses=[wait.pose(i,0) for i in range(wait.count)])
    (romfs/'title-summer.p2m').write_bytes(mesh);report['models']['title-summer']=info
    camera=KeyAnimation(bg['cam.bck']);pose=camera.pose(0,camera.frames-2)
    text=arc['param/param_pikmin.txt']
    def param(key):
        values=re.findall(rb'\{'+key.encode()+rb'\}\s+4\s+([-+0-9.eE]+)',text)
        if len(values)!=1:raise ValueError('Missing title parameter '+key)
        return float(values[0])
    payload=bytearray(struct.pack('<6f',pose[2],pose[5]+15,pose[8],35,param('pk00'),param('pk99')))
    counts=[]
    for name in ('coordinate_eng','coordinate_nintendo'):
        tokens=arc['logo/'+name].split();p=0;row=[]
        for color in range(5):
            n=int(tokens[p]);p+=1
            if not 0<=n<=100:raise ValueError('Title coordinate limit')
            xy=[float(v) for v in tokens[p:p+n*2]];p+=n*2;row.append(n)
            payload.extend(struct.pack('<I',n));payload.extend(struct.pack('<'+str(n*2)+'f',*xy))
        if p!=len(tokens):raise ValueError('Title coordinates trailing data')
        counts.append(row)
    (romfs/'title-scene.bin').write_bytes(b'P2L1'+payload)
    for color in ('blue','red','yellow','black','white'):
        name='title-'+color;model=arc['pikmin/title_'+color+'_piki.bmd']
        mesh,info=convert(model,skinned=True,bake_materials=True,unlit=True);(romfs/(name+'.p2m')).write_bytes(mesh);report['models'][name]=info
        motion,info=animation(model,[(0,'pikmin/wait.bck',arc['pikmin/wait.bck']),(1,'pikmin/wave.bck',arc['pikmin/wave.bck'])])
        (romfs/(name+'.p2a')).write_bytes(motion)
    report['title_scene']={'source':PATHS,'season':'summer','logo_counts':counts,'camera':[pose[2],pose[5]+15,pose[8]],'limitations':'Static environment pose; native arrival steering, not the complete ebi title boids/enemies/seasons engine. Original J2D transforms are sampled; full J2D callbacks/BPK/fog/lighting remain incomplete.'}
