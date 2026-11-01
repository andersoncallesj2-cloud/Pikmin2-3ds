"""Original Valley of Repose geometry/collision, with native spatial indexing."""
import math
import struct
import zlib
from game_archive import rarc
from j3d_model import Block,convert
PATHS=['user/Kando/map/tutorial/arc.szs','user/Kando/map/tutorial/texts.szs']

def build(resources,romfs,report):
    arc=rarc(resources[PATHS[0]]);mesh,info=convert(arc['model.bmd'],max_texture_size=128)
    (romfs/'valley.p2m').write_bytes(mesh);report['models']['valley']=info
    arc=rarc(resources[PATHS[1]]);b=Block(arc['grid.bin']);nv=b.u32(0);base=4+nv*12;nt=b.u32(base);base+=4
    if nv>100000 or nt>20000:raise ValueError('Original collision bounds')
    vertices=[b.read('3f',4+i*12) for i in range(nv)];triangles=[]
    # Retail Sys::Triangle records: indices then precomputed planes, 76 bytes.
    for i in range(nt):
        ids=b.read('3I',base+i*76)
        if any(j>=nv for j in ids):raise ValueError('Original collision index')
        triangles.append(tuple(v for j in ids for v in vertices[j]))
    step=100.;minx=math.floor(min(p[0] for p in vertices)/step)*step;minz=math.floor(min(p[2] for p in vertices)/step)*step
    nx=math.ceil((max(p[0] for p in vertices)-minx)/step)+1;nz=math.ceil((max(p[2] for p in vertices)-minz)/step)+1
    cells=[[] for _ in range(nx*nz)]
    for i,t in enumerate(triangles):
        x0,x1=[int((v-minx)//step) for v in (min(t[0::3]),max(t[0::3]))];z0,z1=[int((v-minz)//step) for v in (min(t[2::3]),max(t[2::3]))]
        for z in range(max(0,z0),min(nz-1,z1)+1):
            for x in range(max(0,x0),min(nx-1,x1)+1):cells[z*nx+x].append(i)
    refs=[];offsets=[0]
    for c in cells:refs.extend(c);offsets.append(len(refs))
    payload=struct.pack('<'+str(nt*9)+'f',*(v for t in triangles for v in t))+struct.pack('<'+str(len(offsets))+'I',*offsets)+struct.pack('<'+str(len(refs))+'I',*refs)
    (romfs/'valley.p2c').write_bytes(struct.pack('<4s4I3fI',b'P2C1',nt,nx,nz,len(refs),step,minx,minz,zlib.crc32(payload))+payload)
    import re
    values=[]
    for line in arc['waterbox.txt'].splitlines():
        part=line.split(b'#')[0];numbers=re.findall(rb'[-+]?\d+\.\d+',part)
        if len(numbers)==6:values.extend(float(v) for v in numbers)
    (romfs/'valley-water.bin').write_bytes(struct.pack('<I',len(values)//6)+struct.pack('<'+str(len(values))+'f',*values))
    report['stage']={'name':'Valley of Repose','source':PATHS,'collision_triangles':nt,'cell_references':len(refs),'texture_limit':128,'placement':'native test entities; original scenario/generators not ported'}
