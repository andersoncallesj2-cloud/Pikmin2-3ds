#!/usr/bin/env python3
"""Build D03 native gameplay demo from a validated personal Pikmin 2 ISO."""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
from build import ROOT, sdk_environment, sha, verify_cia
from disc_asset import extract
from game_archive import rarc
from j3d_model import convert, brk_final_colors
from ui_assets import convert_hud
import menu_art
import world_assets
import audio_assets


def parameter(data, key, duplicate_equal=False):
    matches = re.findall(rb'\{'+key.encode()+rb'\}\s+4\s+([-+0-9.eE]+)', data)
    if len(matches) != 1 and not (duplicate_equal and matches and len(set(matches))==1):
        raise ValueError('Missing/ambiguous gameplay parameter '+key)
    value = float(matches[0])
    if not math.isfinite(value) or value <= 0:
        raise ValueError('Invalid gameplay parameter '+key)
    return value


def run(iso, work_dir=None, makerom=None, tour=False, bannertool=None):
    if tour:raise ValueError('D03 is a manual hardware demo; automated tour is retired')
    if work_dir is None:
        (ROOT/'build').mkdir(exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix='demo-',dir=ROOT/'build'))
    else:
        work = Path(work_dir).resolve(); work.mkdir(parents=True,exist_ok=False)
    (work/'.gitignore').write_text('*\n')
    if any(c.isspace() for c in str(work)): raise ValueError('Choose a build workspace without spaces')
    out=work/'output';out.mkdir()
    report={'demo':'D03','built_at_utc':datetime.now(timezone.utc).isoformat(),
            'hardware':'D03 awaiting user hardware test; no emulator run requested',
            'models':{},'outputs':{},'auto_showcase':tour}
    try:
        paths=['user/Kando/piki/pikis.szs','user/Abe/piki/naviParms.txt',
               'user/Abe/piki/pikiParms.txt','user/Kando/aiConstants.txt','user/Kando/onyon/arc.szs',
               'user/Kando/ufo/arc.szs','user/Abe/Pellet/pal/ahiru.szs','enemy/data/Chappy/model.szs',
               'enemy/data/Chappy/moyou_565.1.bti','enemy/data/Chappy/swallow_565.1.bti',
               'new_screen/eng/res_ground.szs','user/Nishimura/Camera/groundCameraParms.txt',
               'enemy/parm/enemyParms.szs','user/Abe/Pellet/pal/otakara_config.txt','user/Abe/Pellet/pal/carcass_config.txt',
               'user/Yamashita/arc/gameTex.szs','user/Kando/effect/modeleffect.szs']
        resources=dict.fromkeys(paths+world_assets.PATHS+audio_assets.PATHS)
        banner,report['disc']=extract(Path(iso),resources=resources)
        archive=rarc(resources[paths[0]])
        runtime=work/'runtime'
        ignore=shutil.ignore_patterns('build','romfs','*.elf','*.3dsx','*.cia','*.map','*.smdh','__pycache__')
        for folder in ('demo','game_math','include','assets'):
            shutil.copytree(ROOT/'port3ds'/folder,runtime/folder,ignore=ignore)
        app=runtime/'demo';(app/'build').mkdir();romfs=app/'romfs';romfs.mkdir()
        for output,path,tint,mats in [('olimar','orima_model/orima1.bmd',(255,255,255),()),
                                      ('louie','orima_model/orima3.bmd',(255,255,255),()),
                                      ('red','piki_model/piki_p2_red.bmd',(255,30,0),(0,)),
                                      ('leaf','happa_model/leaf.bmd',(255,255,255),())]:
            asset,info=convert(archive[path],tint,mats,skinned=output!='leaf')
            (romfs/(output+'.p2m')).write_bytes(asset)
            report['models'][output]={**info,'archive_path':path,'source_sha256':sha(archive[path]),'converted_sha256':sha(asset)}
        onion_archive=rarc(resources[paths[4]])
        onion=onion_archive['goal.bmd'];colors=brk_final_colors(onion_archive['kidou_red.brk'])
        asset,info=convert(onion,material_colors=colors)
        (romfs/'onion.p2m').write_bytes(asset)
        report['models']['onion']={**info,'archive_path':paths[4]+'/goal.bmd','source_sha256':sha(onion),'converted_sha256':sha(asset)}
        report['onion_red_colour_keys']=colors
        for output,index,name,overrides in [('ship',5,'ufo.bmd',None),('treasure',6,'ahiru.bmd',None),
                                            ('bulborb',7,'enemy.bmd',{0:resources[paths[8]],1:resources[paths[9]]})]:
            data=rarc(resources[paths[index]])[name]
            asset,info=convert(data,external_textures=overrides,skinned=output=='bulborb')
            (romfs/(output+'.p2m')).write_bytes(asset)
            report['models'][output]={**info,'archive_path':paths[index]+'/'+name,'source_sha256':sha(data),'converted_sha256':sha(asset)}
        hud_archive=rarc(resources[paths[10]])
        hud,report['hud_assets'],layout,report['hud_layout']=convert_hud(hud_archive,rarc(resources[paths[15]]))
        (romfs/'hud.p2u').write_bytes(hud)
        (app/'source/hud_layout.h').write_text(layout,encoding='ascii')
        effect=rarc(resources[paths[16]]);colors=brk_final_colors(effect['onyonspot_red.brk'])
        asset,info=convert(effect['onyonspot.bmd'],material_colors=colors,unlit=True)
        (romfs/'spot.p2m').write_bytes(asset)
        report['models']['spot']={**info,'source':'user/Kando/effect/modeleffect.szs/onyonspot.bmd','colour_keys':colors}
        menu_art.create(banner,hud_archive['timg/rp_l64.bti'],app,runtime/'assets')
        world_assets.build(resources,archive,romfs,app/'source',report,parameter)
        audio_assets.build(resources,romfs,report)
        navi,piki=resources[paths[1]],resources[paths[2]]
        gravity=re.findall(rb'(?m)^gravity\s+([0-9.]+)',resources[paths[3]])
        if len(gravity)!=1: raise ValueError('Missing gravity')
        params=[parameter(navi,'p004'),parameter(piki,'p001'),parameter(piki,'p000'),float(gravity[0]),
                parameter(navi,'p025'),parameter(navi,'p026'),parameter(navi,'p001'),parameter(navi,'p053'),parameter(navi,'p002')]
        if not 0<params[3]<2000 or not 0<params[8]<10: raise ValueError('Unsafe parameter range')
        report['parameters']=dict(zip(['captain_speed','leaf_run','walk','gravity','red_throw_height','landing_time','call_max','call_min','call_time'],params))
        # The original Navi table intentionally reuses p048 for two equal values.
        params += [parameter(navi,key,duplicate_equal=(key=='p048')) for key in ('p035','p043','p044','p048','p046','p047','p048')]
        camera=resources[paths[11]]
        groups=re.findall(rb'(?m)^\s*\{\s*\r?\n(.*?)^\s*\}',camera,re.S|re.M)
        if len(groups)<2:raise ValueError('Missing original per-captain camera parameter sections')
        camera_keys=['cnld','cnla','cnlf','cnlo','cnlw','nldt','nlnc','nlfc',
                     'cmld','cmla','cmlf','cmlo','cmlw','mldt','mlnc','mlfc',
                     'cfld','cfla','cflf','cflo','cflw','fldt','flnc','flfc',
                     'cnhd','cnha','cnhf','cnho','cnhw','nhdt','nhnc','nhfc',
                     'cmhd','cmha','cmhf','cmho','cmhw','mhdt','mhnc','mhfc',
                     'cfhd','cfha','cfhf','cfho','cfhw','fhdt','fhnc','fhfc',
                     'zmdt','zman','zmfv','clcr','clms','clmh','clnh','cpmd','cmmt','cmft','cmta','cmtm','cmtb']
        report['camera_parameters']=[]
        for camera in groups[:2]:
            values=[parameter(camera,key) for key in camera_keys]
            params+=values;report['camera_parameters'].append(dict(zip(camera_keys,values)))
        enemy=rarc(resources[paths[12]])['chappy/enemyparm.txt']
        # Enemy general parameters are a separate block from species-specific fpXX keys.
        general=re.findall(rb'(?m)^\s*\{\s*\r?\n(.*?)^\s*\}',enemy,re.S|re.M)
        general=next((g for g in general if b'{fp24}' in g),None)
        if general is None:raise ValueError('Missing original Bulborb general parameters')
        params += [parameter(general,key) for key in ('fp00','fp06','fp11','fp22','fp24')]
        def config(data,name,key):
            records=re.findall(rb'\{([^{}]+)\}',data)
            records=[r for r in records if re.search(rb'\bname\s+'+name.encode()+rb'\s',r)]
            if len(records)!=1:raise ValueError('Missing item config '+name)
            found=re.findall(rb'(?m)^\s*'+key.encode()+rb'\s+([0-9.]+)',records[0])
            if len(found)!=1:raise ValueError('Missing item field '+key)
            return float(found[0])
        treasure,carcass=resources[paths[13]],resources[paths[14]]
        params += [config(treasure,'ahiru','min'),config(treasure,'ahiru','max'),config(carcass,'Chappy','min'),config(carcass,'Chappy','max'),config(treasure,'ahiru','money')]
        report['carry_weights']={'treasure':params[-5:-3],'carcass':params[-3:-1],'treasure_pokos':params[-1]}
        params += [config(carcass,'Chappy','pikicountmin'),parameter(navi,'p050'),parameter(navi,'p051'),parameter(navi,'p052')]
        if len(params)!=152:raise ValueError('Native parameter layout mismatch')
        (romfs/'params.bin').write_bytes(struct.pack('<152f',*params))
        env,compiler=sdk_environment(work)
        report['compiler']=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0]
        def command(args,log):
            with (work/log).open('wb') as f:
                result=subprocess.run([str(a) for a in args],cwd=app,env=env,stdout=f,stderr=subprocess.STDOUT)
            if result.returncode: raise ValueError((work/log).read_text(errors='replace')[-5000:])
        banner_tool=shutil.which(bannertool or 'bannertool')
        if not banner_tool:raise ValueError('bannertool missing; pass --bannertool /path/to/bannertool')
        banner_tool=str(Path(banner_tool).resolve())
        command([banner_tool,'makebanner','-i','../assets/disc-banner.png','-a','../assets/silence.wav','-o','../assets/menu.bnr'],'banner.log')
        report['home_menu_art']={'banner_source':'opening.bnr','icon_source':'new_screen/eng/res_ground.szs/timg/rp_l64.bti','banner_sha256':sha((runtime/'assets/menu.bnr').read_bytes()),'icon_png_sha256':sha((app/'icon.png').read_bytes())}
        command(['make','-j2','TOUR='+str(int(tour))],'compile.log')
        smdh=(app/'pikmin2-demo.smdh').read_bytes()
        report['smdh_title']=smdh[8:136].decode('utf-16le').split('\0',1)[0]
        if smdh[:4]!=b'SMDH' or report['smdh_title']!='Pikmin 2 - 3DS port':raise ValueError('HOME Menu title mismatch')
        elf=(app/'pikmin2-demo.elf').read_bytes()
        if elf[:6]!=b'\x7fELF\x01\x01' or struct.unpack_from('<H',elf,18)[0]!=40: raise ValueError('Expected ARM ELF')
        x3ds=(app/'pikmin2-demo.3dsx').read_bytes()
        if x3ds[:4]!=b'3DSX' or any((romfs/name).read_bytes() not in x3ds for name in ('red.p2m','onion.p2m')): raise ValueError('3DSX missing model')
        shutil.copy2(app/'pikmin2-demo.3dsx',out/'Pikmin2-3DS-port-D03.3dsx')
        packager=shutil.which(makerom or 'makerom')
        if not packager: raise ValueError('makerom missing; pass --makerom')
        command([str(Path(packager).resolve()),'-f','cia','-o',out/'Pikmin2-3DS-port-D03.cia','-rsf','demo.rsf','-target','t','-elf','pikmin2-demo.elf','-icon','pikmin2-demo.smdh','-banner','../assets/menu.bnr','-desc','app:4'],'package.log')
        report['cia_checks']=verify_cia(out/'Pikmin2-3DS-port-D03.cia',(romfs/'red.p2m').read_bytes())
        for file in romfs.iterdir():verify_cia(out/'Pikmin2-3DS-port-D03.cia',file.read_bytes())
        report['source_hashes']={str(f.relative_to(runtime)):sha(f.read_bytes()) for f in runtime.rglob('*') if f.is_file() and f.suffix in ('.c','.cpp','.h','.pica')}
        for f in out.iterdir():report['outputs'][f.name]={'sha256':sha(f.read_bytes()),'bytes':f.stat().st_size}
        report['result']='PASS: ARM compilation, real disc conversion, CIA/3DSX packaging'
    except Exception as exc:
        report['result']='FAILED';report['error']=str(exc);raise
    finally:(out/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['result']);print(out)
    return work


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--iso',type=Path,required=True)
    p.add_argument('--work-dir',type=Path);p.add_argument('--makerom');p.add_argument('--tour',action='store_true')
    p.add_argument('--bannertool')
    a=p.parse_args()
    try:run(a.iso,a.work_dir,a.makerom,a.tour,a.bannertool)
    except (ValueError,OSError,subprocess.CalledProcessError) as exc:p.exit(1,f'Demo build failed: {exc}\n')
