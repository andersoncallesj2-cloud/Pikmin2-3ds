"""Bounded static J2D BLO2 picture subset for original ground HUD conversion.

Format/transform references: projectPiki J2DPane::makePaneExStream,
J2DPictureEx constructor/drawTexCoord and J2DMaterialFactory. New converter;
does not implement the complete J2D scene/animation engine.
"""
import math
from j3d_model import Block


def read_layout(data):
    if data[:8]!=b'SCRNblo2' or len(data)>1024*1024:raise ValueError('Expected bounded BLO2')
    source=Block(data);offset=32;chunks=[]
    count=source.u32(12)
    if not 1<=count<=4096:raise ValueError('BLO chunk limit exceeded')
    for _ in range(count):
        tag,size=source.read('4sI',offset)
        if size<8:raise ValueError('Invalid BLO chunk length')
        chunks.append((tag,Block(source.get(offset,size))));offset+=size
    refs=[];material=None
    for tag,b in chunks:
        if tag==b'TEX1':
            base=b.u32(12);count=b.u16(base)
            if count>256:raise ValueError('Too many BLO references')
            for i in range(count):
                p=base+b.u16(base+2+i*2);kind,size=b.read('2B',p)
                if kind not in (2,3,4):raise ValueError('Unsupported resource reference')
                refs.append(b.get(p+2,size).decode('ascii').replace('\\','/').split('/')[-1])
        elif tag==b'MAT1':material=b
    if material is None:raise ValueError('Missing BLO materials')
    def mat(index):
        b=material
        if index>=b.u16(8):raise ValueError('Invalid BLO material index')
        p=b.u32(12)+b.u16(b.u32(16)+index*2)*232
        ti=b.u16(p+56)
        if ti==65535:return None
        texture=b.u16(b.u32(56)+ti*2)
        if texture>=len(refs):raise ValueError('BLO texture index outside table')
        black=(0,0,0,0);white=(255,255,255,255)
        texgens=b.get(b.u32(44)+b.get(p+3,1)[0],1)[0]
        stages=b.get(b.u32(76)+b.get(p+4,1)[0],1)[0]
        if (texgens==1 and stages!=1) or (texgens!=1 and stages!=texgens+1):
            colors=[]
            for c in range(2):colors.append(tuple(v&255 for v in b.read('4h',b.u32(68)+b.u16(p+146+2*c)*8)))
            black,white=colors
        color=b.get(b.u32(32)+b.u16(p+8)*4,4)
        return refs[texture],black,white,tuple(color)
    # Matrix is (xx,xy,tx,yx,yy,ty); pane anchors affect bounds, not children.
    identity=(1,0,0,0,1,0);current=(identity,True);last=current;stack=[];pictures=[];panes={}
    def concat(a,b):
        return (a[0]*b[0]+a[1]*b[3],a[0]*b[1]+a[1]*b[4],a[0]*b[2]+a[1]*b[5]+a[2],
                a[3]*b[0]+a[4]*b[3],a[3]*b[1]+a[4]*b[4],a[3]*b[2]+a[4]*b[5]+a[5])
    for tag,b in chunks:
        if tag==b'BGN1':stack.append(current);current=last
        elif tag==b'END1':
            if not stack:raise ValueError('Unbalanced BLO tree')
            current=stack.pop()
        elif tag in (b'PAN2',b'PIC2',b'TBX2'):
            q=0 if tag==b'PAN2' else 8
            visible,anchor=b.read('2B',q+12);name=b.get(q+16,8).strip(b'\0').decode('ascii')
            w,h,sx,sy,rx,ry,rz,x,y=b.read('9f',q+32)
            if any(not math.isfinite(v) for v in (w,h,sx,sy,rx,ry,rz,x,y)) or rx or ry:raise ValueError('Unsupported BLO pane transform')
            angle=rz*math.pi/180;c=math.cos(angle);s=math.sin(angle)
            matrix=concat(current[0],(c*sx,-s*sy,x,s*sx,c*sy,y));last=(matrix,current[1] and bool(visible))
            bounds=(-w*(anchor%3)*.5,-h*(anchor//3)*.5)
            points=[]
            for px,py in ((0,0),(w,0),(0,h),(w,h)):
                px+=bounds[0];py+=bounds[1];points.append((matrix[0]*px+matrix[1]*py+matrix[2],matrix[3]*px+matrix[4]*py+matrix[5]))
            record={'tag':name,'points':points,'visible':last[1]}
            panes[name]=record
            if tag==b'PIC2':
                trailer=q+b.u32(q+4);info=mat(b.u16(trailer+4))
                if info is None:continue
                texture,black,white,color=info
                record={**record,'texture':texture,'black':black,'white':white,'material_color':color,
                        'uv':[v/256 for v in b.read('8h',trailer+16)],
                        'colors':[tuple(b.get(trailer+32+i*4,4)) for i in range(4)]}
                pictures.append(record)
    return pictures,panes
