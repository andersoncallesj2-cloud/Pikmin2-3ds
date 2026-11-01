#!/usr/bin/env python3
"""Build D01 native gameplay demo from a validated personal Pikmin 2 ISO."""
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
from j3d_model import convert


def parameter(data, key):
    matches = re.findall(rb'\{'+key.encode()+rb'\}\s+4\s+([-+0-9.eE]+)', data)
    if len(matches) != 1:
        raise ValueError('Missing/ambiguous gameplay parameter '+key)
    value = float(matches[0])
    if not math.isfinite(value) or value <= 0:
        raise ValueError('Invalid gameplay parameter '+key)
    return value


def run(iso, work_dir=None, makerom=None, tour=False):
    if work_dir is None:
        (ROOT/'build').mkdir(exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix='demo-',dir=ROOT/'build'))
    else:
        work = Path(work_dir).resolve(); work.mkdir(parents=True,exist_ok=False)
    (work/'.gitignore').write_text('*\n')
    if any(c.isspace() for c in str(work)): raise ValueError('Choose a build workspace without spaces')
    out=work/'output';out.mkdir()
    report={'demo':'D01','built_at_utc':datetime.now(timezone.utc).isoformat(),
            'hardware':'D01 awaiting test; HW03 GPU control passed per user',
            'models':{},'outputs':{},'auto_showcase':tour}
    try:
        paths=['user/Kando/piki/pikis.szs','user/Abe/piki/naviParms.txt',
               'user/Abe/piki/pikiParms.txt','user/Kando/aiConstants.txt','user/Kando/onyon/arc.szs']
        resources=dict.fromkeys(paths)
        _,report['disc']=extract(Path(iso),resources=resources)
        archive=rarc(resources[paths[0]])
        runtime=work/'runtime'
        ignore=shutil.ignore_patterns('build','romfs','*.elf','*.3dsx','*.cia','*.map','*.smdh','__pycache__')
        for folder in ('demo','game_math','include','assets'):
            shutil.copytree(ROOT/'port3ds'/folder,runtime/folder,ignore=ignore)
        app=runtime/'demo';(app/'build').mkdir();romfs=app/'romfs';romfs.mkdir()
        for output,path,tint,mats in [('olimar','orima_model/orima1.bmd',(255,255,255),()),
                                      ('red','piki_model/piki_p2_red.bmd',(255,30,0),(0,)),
                                      ('leaf','happa_model/leaf.bmd',(255,255,255),())]:
            asset,info=convert(archive[path],tint,mats)
            (romfs/(output+'.p2m')).write_bytes(asset)
            report['models'][output]={**info,'archive_path':path,'source_sha256':sha(archive[path]),'converted_sha256':sha(asset)}
        onion=rarc(resources[paths[4]])['goal.bmd']
        asset,info=convert(onion)
        (romfs/'onion.p2m').write_bytes(asset)
        report['models']['onion']={**info,'archive_path':paths[4]+'/goal.bmd','source_sha256':sha(onion),'converted_sha256':sha(asset)}
        navi,piki=resources[paths[1]],resources[paths[2]]
        gravity=re.findall(rb'(?m)^gravity\s+([0-9.]+)',resources[paths[3]])
        if len(gravity)!=1: raise ValueError('Missing gravity')
        params=[parameter(navi,'p004'),parameter(piki,'p001'),parameter(piki,'p000'),float(gravity[0]),
                parameter(navi,'p025'),parameter(navi,'p026'),parameter(navi,'p001'),parameter(navi,'p053'),parameter(navi,'p002')]
        if not 0<params[3]<2000 or not 0<params[8]<10: raise ValueError('Unsafe parameter range')
        report['parameters']=dict(zip(['captain_speed','leaf_run','walk','gravity','red_throw_height','landing_time','call_max','call_min','call_time'],params))
        (romfs/'params.bin').write_bytes(struct.pack('<9f',*params))
        env,compiler=sdk_environment(work)
        report['compiler']=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0]
        def command(args,log):
            with (work/log).open('wb') as f:
                result=subprocess.run([str(a) for a in args],cwd=app,env=env,stdout=f,stderr=subprocess.STDOUT)
            if result.returncode: raise ValueError((work/log).read_text(errors='replace')[-5000:])
        command(['make','-j2','TOUR='+str(int(tour))],'compile.log')
        elf=(app/'pikmin2-demo.elf').read_bytes()
        if elf[:6]!=b'\x7fELF\x01\x01' or struct.unpack_from('<H',elf,18)[0]!=40: raise ValueError('Expected ARM ELF')
        x3ds=(app/'pikmin2-demo.3dsx').read_bytes()
        if x3ds[:4]!=b'3DSX' or any((romfs/name).read_bytes() not in x3ds for name in ('red.p2m','onion.p2m')): raise ValueError('3DSX missing model')
        shutil.copy2(app/'pikmin2-demo.3dsx',out/'Pikmin2-Demo-D01.3dsx')
        packager=shutil.which(makerom or 'makerom')
        if not packager: raise ValueError('makerom missing; pass --makerom')
        command([str(Path(packager).resolve()),'-f','cia','-o',out/'Pikmin2-Demo-D01.cia','-rsf','demo.rsf','-target','t','-elf','pikmin2-demo.elf','-icon','pikmin2-demo.smdh','-banner','../assets/menu.bnr','-desc','app:4'],'package.log')
        report['cia_checks']=verify_cia(out/'Pikmin2-Demo-D01.cia',(romfs/'red.p2m').read_bytes())
        verify_cia(out/'Pikmin2-Demo-D01.cia',(romfs/'onion.p2m').read_bytes())
        report['source_hashes']={str(f.relative_to(runtime)):sha(f.read_bytes()) for f in runtime.rglob('*') if f.is_file() and f.suffix in ('.c','.h','.pica')}
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
    a=p.parse_args()
    try:run(a.iso,a.work_dir,a.makerom,a.tour)
    except (ValueError,OSError,subprocess.CalledProcessError) as exc:p.exit(1,f'Demo build failed: {exc}\n')
