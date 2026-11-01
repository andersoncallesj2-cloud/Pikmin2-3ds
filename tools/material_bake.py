"""Bake supported original MAT3 TEV stages to a native base texture.

New adapter of GX's D +/- lerp(A,B,C), using original stage orders, signed
registers, texture matrices and colour keys. View-dependent reflection uses
a fixed neutral lookup; this is not a full dynamic GX renderer.
"""
import math
from j3d_model import bti

def bake(mat,r,tex,override=None):
    rawregs=[list(mat.read('4h',mat.u32(80)+mat.u16(r+220+i*2)*8)) for i in range(4)]
    if override:rawregs[0]=list(override)
    stages=[];images={};width=height=8
    for i in range(mat.get(mat.u32(88)+mat.get(r+4,1)[0],1)[0]):
        st=mat.get(mat.u32(92)+mat.u16(r+228+i*2)*20,20)
        coord,slot,channel,_=mat.get(mat.u32(76)+mat.u16(r+188+i*2)*4,4)
        image=None;transform=None;reflection=False
        if slot<8:
            ti=mat.u16(r+132+slot*2)
            if ti!=65535:
                idx=mat.u16(mat.u32(72)+ti*2)
                if idx not in images:images[idx]=bti(tex.data[tex.u32(12)+idx*32:])
                image=images[idx];width=max(width,image[0]);height=max(height,image[1])
            if coord<8:
                gi=mat.u16(r+40+coord*2)
                if gi!=65535:
                    _,source,matrix,_=mat.get(mat.u32(56)+gi*4,4);reflection=source==1
                    if matrix!=60 and 30<=matrix<=57:
                        mi=mat.u16(r+72+(matrix-30)//3*2)
                        if mi!=65535:
                            p=mat.u32(64)+mi*100;cx,cy,_=mat.read('3f',p+4);sx,sy=mat.read('2f',p+16);angle=mat.read('h',p+24)[0]*math.pi/32768;tx,ty=mat.read('2f',p+28)
                            co,si=math.cos(angle),math.sin(angle);transform=(sx*co,-sx*si,(-sx*co*cx+sx*si*cy)+cx+tx,sy*si,sy*co,(-sy*si*cx-sy*co*cy)+cy+ty)
        stages.append((st,image,transform,reflection,channel))
    width=min(256,width);height=min(512,height);out=bytearray(width*height*4)
    raster=mat.get(mat.u32(32)+mat.u16(r+8)*4,4)
    def op(a,b,c,d,mode,bias,scale,clamp):
        value=d+(-1 if mode==1 else 1)*(a*(1-c/255)+b*c/255)
        value=(value+(128 if bias==1 else -128 if bias==2 else 0))*(1,2,4,.5)[scale]
        return max(0,min(255,value)) if clamp else max(-1024,min(1023,value))
    for y in range(height):
        for x in range(width):
            regs=[list(rawregs[3])]+[list(c) for c in rawregs[:3]]
            for st,image,tr,reflection,ch in stages:
                u,v=(x+.5)/width,(y+.5)/height
                if reflection:u=v=.5
                elif tr:u,v=tr[0]*u+tr[1]*v+tr[2],tr[3]*u+tr[4]*v+tr[5]
                if image:
                    w,h,pix=image;px=max(0,min(w-1,int(u*w)));py=max(0,min(h-1,int(v*h)));color=pix[(py*w+px)*4:(py*w+px)*4+4]
                else:color=(255,255,255,255)
                ras=raster if ch==4 else (0,0,0,0)
                def ci(code,c):
                    if code<8:return regs[code//2][3 if code%2 else c]
                    return (color[c],color[3],ras[c],ras[3],255,128,255,0)[code-8]
                def ai(code):return regs[code][3] if code<4 else (color[3],ras[3],255,0)[code-4]
                rgb=[op(*(ci(st[k],c) for k in (1,2,3,4)),*st[5:9]) for c in range(3)]
                alpha=op(*(ai(st[k]) for k in (10,11,12,13)),*st[14:18])
                regs[st[9]][:3]=rgb;regs[st[18]][3]=alpha
            p=(y*width+x)*4;out[p:p+4]=bytes(max(0,min(255,round(v))) for v in regs[0])
    return width,height,bytes(out)
