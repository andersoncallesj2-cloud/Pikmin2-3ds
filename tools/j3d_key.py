"""Bounded original BCK key tracks, adapted from J3D/J2D transform sampling.

Reference: projectPiki 999140f, J3DAnimation.cpp/J2DAnimation.cpp. Native
conversion glue; this is not the original animation state machine.
"""
import math
from j3d_model import Block


def model_worlds(data, poses=None):
    from j3d_animation import chunks
    from j3d_model import multiply
    c=chunks(data,b'J3D2bmd3');j=c[b'JNT1'];inf=c[b'INF1'];n=j.u16(8)
    parent=-1;last=-1;stack=[];parents={};p=inf.u32(20)
    while True:
        kind,index=inf.read('2H',p);p+=4
        if kind==0:break
        if kind==1:stack.append(parent);parent=last
        elif kind==2:parent=stack.pop();last=parent
        elif kind==16:parents[index]=parent;last=index
    values=[];comp=[]
    for i in range(n):
        row=j.u32(12)+j.u16(j.u32(16)+2*i)*64;comp.append(j.get(row+2,1)[0]==1)
        sx,sy,sz,rx,ry,rz,x,y,z=j.read('3f3h2x3f',row+4)
        values.append(poses[i] if poses is not None else [sx,rx,x,sy,ry,y,sz,rz,z])
    out={}
    def world(i):
        if i in out:return out[i]
        v=values[i];sx,sy,sz=(math.sin(v[k]*math.pi/32768) for k in (1,4,7));cx,cy,cz=(math.cos(v[k]*math.pi/32768) for k in (1,4,7))
        local=[[cz*cy,sx*cz*sy-cx*sz,cx*cz*sy+sx*sz,v[2]],
               [sz*cy,sx*sz*sy+cx*cz,cx*sz*sy-sx*cz,v[5]],[-sy,cy*sx,cy*cx,v[8]],[0,0,0,1]]
        parent=parents[i]
        for row in range(3):
            for col in range(3):local[row][col]*=v[col*3]
            if parent>=0 and comp[i]:
                scale=values[parent][row*3]
                if abs(scale)<1e-8:raise ValueError('Zero parent scale')
                for col in range(3):local[row][col]/=scale
        out[i]=multiply(world(parent),local) if parent>=0 else local
        return out[i]
    return [world(i) for i in range(n)]


class KeyAnimation:
    def __init__(self, data):
        if data[:8] not in (b'J3D1bck1', b'J3D1bck2'):
            raise ValueError('Expected BCK key animation')
        source=Block(data);size=source.u32(36)
        if data[32:36]!=b'ANK1' or size<36 or size>len(data)-32+31:
            raise ValueError('Invalid BCK ANK1')
        self.b=Block(data[32:32+size]);b=self.b
        self.frames=b.u16(10);self.count=b.u16(12);self.loop=b.get(8,1)[0] in (2,3,4)
        self.shift=b.get(9,1)[0]
        if not 0<self.frames<=2000 or not 0<self.count<=1024 or self.shift>16:
            raise ValueError('BCK key limits')
        table,self.scale,self.rotation,self.translation=b.read('4I',20)
        self.tracks=[b.read('27H',table+i*54) for i in range(self.count)]

    def value(self, track, frame, component):
        n,start,mode=track
        if not 0<n<=2000 or mode>1:raise ValueError('Invalid BCK key track')
        base=(self.scale,self.rotation,self.translation)[component]
        fmt,size=('h',2) if component==1 else ('f',4)
        def get(i):
            v=self.b.read(fmt,base+(start+i)*size)[0]
            if not math.isfinite(v):raise ValueError('Nonfinite BCK key')
            return v
        if n==1:result=get(0)
        else:
            stride=3+mode
            if frame<=get(0):result=get(1)
            elif frame>=get((n-1)*stride):result=get((n-1)*stride+1)
            else:
                for i in range(n-1):
                    a=i*stride;c=(i+1)*stride;t0,t1=get(a),get(c)
                    if t0<=frame<t1:
                        dt=t1-t0
                        if dt<=0:raise ValueError('Unsorted BCK keys')
                        t=(frame-t0)/dt;t2=t*t;t3=t2*t
                        result=(2*t3-3*t2+1)*get(a+1)+(t3-2*t2+t)*dt*get(a+2+mode)+(-2*t3+3*t2)*get(c+1)+(t3-t2)*dt*get(c+2)
                        break
                else:raise ValueError('BCK key interval')
        return result*(1<<self.shift) if component==1 else result

    def pose(self, index, frame):
        t=self.tracks[index]
        return [self.value(t[axis*9+component*3:axis*9+component*3+3],frame,component)
                for axis in range(3) for component in range(3)]
