"""BCA full-frame animation -> checked, precomputed native matrix palettes.

Adapted from projectPiki J3DAnmTransformFull::getTransform, J3DJoint Maya
scale compensation and J3DGetTranslateRotateMtx (CC0, 999140f). Original
frames and envelope weights are retained. Native PICA draws immutable VBOs;
only matrix uniforms change. This does not implement animation blending,
callbacks, texture/colour animation, or the game's complete motion FSMs.
"""
import math
import struct
import zlib
from j3d_model import Block,multiply


def chunks(data,signature):
    b=Block(data)
    if data[:8]!=signature or not 1<=b.u32(12)<=32:raise ValueError('Animation/model signature')
    p=32;result={}
    for _ in range(b.u32(12)):
        tag,size=b.read('4sI',p)
        if size<8 or tag in result:raise ValueError('Invalid animation/model chunk')
        # Retail RARC BCA entries omit up to 31 trailing alignment bytes.
        extent=min(size,len(data)-p)
        if extent<size and (signature!=b'J3D1bca1' or size-extent>31):raise ValueError('Truncated animation chunk')
        result[tag]=Block(b.get(p,extent));p+=size
    return result


def convert(model,clips):
    c=chunks(model,b'J3D2bmd3');jnt,inf,drw,evp=(c[k] for k in (b'JNT1',b'INF1',b'DRW1',b'EVP1'))
    nj=jnt.u16(8);nm=drw.u16(8)
    if not 0<nm<=29 or not 0<nj<=64:raise ValueError('Animated skeleton exceeds native limits')
    parents={};stack=[];parent=-1;last=-1;p=inf.u32(20)
    for _ in range(8192):
        kind,index=inf.read('2H',p);p+=4
        if kind==0:break
        if kind==1:stack.append(parent);parent=last
        elif kind==2:parent=stack.pop();last=parent
        elif kind==16:parents[index]=parent;last=index
    if len(parents)!=nj:raise ValueError('Incomplete animation hierarchy')
    compensate=[jnt.get(jnt.u32(12)+jnt.u16(jnt.u32(16)+i*2)*64+2,1)[0]==1 for i in range(nj)]
    payload=bytearray();info=[]
    for state,label,data in clips:
        b=chunks(data,b'J3D1bca1')[b'ANF1'];frames=b.u16(10);count=b.u16(12)
        if count!=nj or not 0<frames<=600:raise ValueError('BCA skeleton/frame mismatch: '+label)
        table,scale,rotation,translation=b.read('4I',20)
        tracks=[b.read('18H',table+i*36) for i in range(nj)]
        loop=b.get(8,1)[0] in (2,3,4) and state not in (5,10)
        payload.extend(struct.pack('<4I',state,frames,int(loop),nm))
        for frame in range(frames):
            poses=[]
            for t in tracks:
                values=[]
                for axis in range(3):
                    for component,base,fmt,size in ((0,scale,'f',4),(1,rotation,'h',2),(2,translation,'f',4)):
                        n,start=t[axis*6+component*2:axis*6+component*2+2]
                        if n<1:raise ValueError('Empty BCA track')
                        value=b.read(fmt,base+(start+min(frame,n-1))*size)[0]
                        if not math.isfinite(value):raise ValueError('Nonfinite BCA sample')
                        values.append(value)
                poses.append(values)
            worlds={}
            def world(i):
                if i in worlds:return worlds[i]
                v=poses[i];sx,sy,sz=(math.sin(v[k]*math.pi/32768) for k in (1,4,7));cx,cy,cz=(math.cos(v[k]*math.pi/32768) for k in (1,4,7))
                local=[[cz*cy,sx*cz*sy-cx*sz,cx*cz*sy+sx*sz,v[2]],
                       [sz*cy,sx*sz*sy+cx*cz,cx*sz*sy-sx*cz,v[5]],
                       [-sy,cy*sx,cy*cx,v[8]],[0,0,0,1]]
                for row in range(3):
                    for col in range(3):local[row][col]*=v[col*3]
                p=parents[i]
                if p>=0 and compensate[i]:
                    for row in range(3):
                        ps=poses[p][row*3]
                        if abs(ps)<1e-8:raise ValueError('Zero parent scale')
                        for col in range(3):local[row][col]/=ps
                worlds[i]=multiply(world(p),local) if p>=0 else local
                return worlds[i]
            envelopes=[];cursor=0
            for e in range(evp.u16(8)):
                n=evp.get(evp.u32(12)+e,1)[0];blend=[[0.]*4 for _ in range(4)]
                for k in range(n):
                    joint=evp.u16(evp.u32(16)+2*(cursor+k));weight=evp.read('f',evp.u32(20)+4*(cursor+k))[0]
                    rows=evp.read('12f',evp.u32(24)+joint*48);inv=[list(rows[r*4:r*4+4]) for r in range(3)]+[[0,0,0,1]]
                    mat=multiply(world(joint),inv)
                    for r in range(4):
                        for col in range(4):blend[r][col]+=weight*mat[r][col]
                cursor+=n;envelopes.append(blend)
            for m in range(nm):
                flag=drw.get(drw.u32(12)+m,1)[0];index=drw.u16(drw.u32(16)+m*2)
                mat=world(index) if flag==0 else envelopes[index]
                # PICA uniform vector storage is w,z,y,x in CPU memory.
                for row in mat[:3]:payload.extend(struct.pack('<4f',*row[::-1]))
        info.append({'state':state,'source':label,'frames':frames,'loop':loop})
    return struct.pack('<4s5I',b'P2A1',len(payload),zlib.crc32(payload),len(clips),nm,nj)+payload,info
