"""Original Valley base camp and model-space storage attachment points.

References: Generator::read, CourseInfo::read, Onyon::{getLegPart,getFootPart,
getInStart_UFO,getOutStart_UFO}, BaseGameSection captain initialization.
Only the base-camp generator subset is imported; this is not a scenario loader.
"""
import re,struct,math
from game_archive import rarc
from j3d_key import model_worlds
from j3d_animation import chunks
from j3d_model import names,transform
PATHS=['user/Abe/stages.txt','user/Abe/map/tutorial/defaultgen.txt','user/Kando/onyon/onyonColl.txt']

def build(resources,romfs,report):
    def clean(data):return re.sub(rb'#[^\n]*',b'',data.replace(b'\r',b''))
    stages=clean(resources[PATHS[0]]);course=re.search(rb'\bname\s+tutorial\b(.*?)\bend\b',stages,re.S)
    if not course:raise ValueError('Valley course missing')
    xyz=re.search(rb'\bstart\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)',course[1]);angle=re.search(rb'\bstartangle\s+([-+0-9.eE]+)',course[1])
    if not xyz or not angle:raise ValueError('Valley start missing')
    spawn=[*(float(v) for v in xyz.groups()),math.radians(float(angle[1]))]
    tokens=re.findall(rb'\{[^{}\s]+\}|[{}]|[^\s{}]+',clean(resources[PATHS[1]]));p=6;records=[]
    for _ in range(int(tokens[5])):
        if tokens[p]!=b'{':raise ValueError('Generator block missing')
        start=p;p+=1;depth=1
        while depth:
            if tokens[p]==b'{':depth+=1
            elif tokens[p]==b'}':depth-=1
            p+=1
        record=tokens[start+1:p-1]
        if record[0]!=b'{v0.3}':raise ValueError('Unsupported generator revision')
        if record[41]!=b'{item}':continue
        if b'{onyn}' not in record:continue
        j=record.index(b'{onyn}');kind=int(record[j+5]);boot=int(record[j+6])
        if not boot:continue
        pos=[float(record[35+i])+float(record[38+i]) for i in range(3)];rot=math.radians(float(record[j+2]));records.append((kind,[*pos,rot]))
    landmarks=[spawn]
    for kind in (0,1,2,4):
        choices=[pos for k,pos in records if k==kind]
        if len(choices)!=1:raise ValueError('Ambiguous original base camp '+str(kind))
        landmarks.append(choices[0])
    model=rarc(resources['user/Kando/onyon/arc.szs'])['goal.bmd'];world=model_worlds(model)
    t=clean(resources[PATHS[2]]).split();p=0;points={}
    def node():
        nonlocal p
        count=int(t[p]);p+=2;name=t[p].strip(b'{}').decode();p+=2;v=[float(x) for x in t[p:p+3]];p+=3;joint=int(t[p]);p+=2;points[name]=transform(world[joint],v)
        if count:
            if t[p]!=b'{':raise ValueError('Collision children missing')
            p+=1
            for _ in range(count):node()
            if t[p]!=b'}':raise ValueError('Collision children unterminated')
            p+=1
    node();attachments=[points[tag] for tag in ('leg1','fot1','leg2','fot2','leg3','fot3')]
    ship=rarc(resources['user/Kando/ufo/arc.szs'])['ufo.bmd'];j=chunks(ship,b'J3D2bmd3')[b'JNT1'];labels=names(j,j.u32(20));world=model_worlds(ship)
    attachments += [transform(world[labels.index(tag)],(0,0,0)) for tag in ('in1','out','start1')]
    values=[v for row in landmarks for v in row]+[v for row in attachments for v in row]
    if not all(math.isfinite(v) for v in values):raise ValueError('Invalid storage point')
    (romfs/'storage.bin').write_bytes(b'P2N1'+struct.pack('<47f',*values))
    report['base_camp']={'source':PATHS,'placements':dict(zip(('course_start','blue','red','yellow','ship'),landmarks)),'attachments':'Original onyonColl.txt transformed through goal.bmd joints; ufo.bmd in1/out/start1','spawn_algorithm':'BaseGameSection demo matrix offsets (-40,0,2) and (-60,0,-10); MapMgr rotation from stages.txt'}
