"""Bounded bind-pose BMD -> native textured triangle batches.

New converter; format references are projectPiki J3DFileBlock/J3DJointFactory,
J3DShapeFactory and J3DGetTranslateRotateMtx. No original assets in this file.
Supports rigid and weighted bind poses; rejects unsupported GX vertex formats.
Materials approximate base colour/first texture, not the full GX TEV pipeline.
Texture matrix index bytes are consumed; animated/reflection UV transforms are
not implemented. Native padding preserves the first image without resampling.
"""
import math
import struct
import zlib


class Block:
    def __init__(self, data):
        self.data = data

    def get(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise ValueError('BMD field outside chunk')
        return self.data[offset:offset+size]

    def read(self, fmt, offset):
        return struct.unpack('>'+fmt, self.get(offset, struct.calcsize('>'+fmt)))

    def u16(self, o): return self.read('H', o)[0]
    def u32(self, o): return self.read('I', o)[0]


def texture(fmt, width, height, data):
    """GX block order -> row-major RGBA, preserving original pixel dimensions."""
    if fmt == 14:
        required = ((width+7)//8)*((height+7)//8)*32
        if not 1 <= width <= 1024 or not 1 <= height <= 1024 or len(data) < required:
            raise ValueError('Invalid CMPR extent')
        pixels = bytearray(width*height*4); offset = 0
        def rgb(v): return ((v >> 11)*255//31,(v >> 5 & 63)*255//63,(v & 31)*255//31,255)
        for by in range(0,height,8):
            for bx in range(0,width,8):
                for sy,sx in ((0,0),(0,4),(4,0),(4,4)):
                    a,b,bits = struct.unpack_from('>HHI',data,offset); offset += 8
                    c0,c1 = rgb(a),rgb(b)
                    colors = [c0,c1]
                    colors += [tuple((2*c0[i]+c1[i])//3 for i in range(4)),tuple((c0[i]+2*c1[i])//3 for i in range(4))] if a>b else [tuple((c0[i]+c1[i])//2 for i in range(4)),(0,0,0,0)]
                    for y in range(4):
                        for x in range(4):
                            if bx+sx+x < width and by+sy+y < height:
                                p = ((by+sy+y)*width+bx+sx+x)*4
                                pixels[p:p+4] = bytes(colors[bits >> (30-2*(y*4+x)) & 3])
        return bytes(pixels)
    shapes = {0: (8, 8, 32), 1: (8, 4, 32), 2: (8, 4, 32),
              3: (4, 4, 32), 4: (4, 4, 32), 5: (4, 4, 32), 6: (4, 4, 64)}
    if fmt not in shapes or not 1 <= width <= 1024 or not 1 <= height <= 1024:
        raise ValueError('Unsupported GX texture format/dimensions')
    bw, bh, bs = shapes[fmt]
    required = ((width+bw-1)//bw)*((height+bh-1)//bh)*bs
    if len(data) < required:
        raise ValueError('Truncated GX texture')
    pixels = bytearray(width*height*4)
    offset = 0
    for by in range(0, height, bh):
        for bx in range(0, width, bw):
            block = data[offset:offset+bs]
            offset += bs
            for y in range(bh):
                for x in range(bw):
                    i = y*bw+x
                    if fmt == 0:
                        v = (block[i//2] >> (4 if i%2 == 0 else 0) & 15)*17
                        rgba = (v, v, v, v)
                    elif fmt == 1:
                        v = block[i]; rgba = (v, v, v, v)
                    elif fmt == 2:
                        v = (block[i] & 15)*17; rgba = (v, v, v, (block[i] >> 4)*17)
                    elif fmt == 6:
                        rgba = (block[i*2+1], block[32+i*2], block[33+i*2], block[i*2])
                    else:
                        v = int.from_bytes(block[i*2:i*2+2], 'big')
                        if fmt == 3:
                            rgba = (v & 255,)*3+(v >> 8,)
                        elif fmt == 4:
                            rgba = (((v >> 11)*255//31), ((v >> 5 & 63)*255//63), ((v & 31)*255//31), 255)
                        elif v & 0x8000:
                            rgba = ((v >> 10 & 31)*255//31, (v >> 5 & 31)*255//31, (v & 31)*255//31, 255)
                        else:
                            rgba = ((v >> 8 & 15)*17, (v >> 4 & 15)*17, (v & 15)*17, (v >> 12)*255//7)
                    if bx+x < width and by+y < height:
                        p = ((by+y)*width+bx+x)*4
                        pixels[p:p+4] = bytes(rgba)
    return bytes(pixels)


def tiled_rgba(pixels, width, height):
    """3DS RGBA8: 8x8 Morton tiles, ABGR bytes; source top row at native v=1.

    PICA's v axis is inverted relative to its tile row order. Do not flip the
    pixels again: native quads already convert GX v with (1-v). D02 did both,
    inverting glyphs and sampling the padding instead of the portrait's face.
    """
    if width < 8 or height < 8 or width & (width-1) or height & (height-1):
        raise ValueError('Native textures require power-of-two dimensions >=8')
    out = bytearray(len(pixels))
    for y in range(height):
        for x in range(width):
            morton = sum(((x >> b & 1) << (2*b)) | ((y >> b & 1) << (2*b+1)) for b in range(3))
            dst = ((y//8)*(width//8)*64+(x//8)*64+morton)*4
            src = (y*width+x)*4
            out[dst:dst+4] = pixels[src:src+4][::-1]
    return bytes(out)


def multiply(a, b):
    return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def transform(m, v, w=1):
    return tuple(sum(m[i][j]*v[j] for j in range(3))+m[i][3]*w for i in range(3))


def names(block, base):
    count=block.u16(base)
    if count>4096: raise ValueError('Name table too large')
    result=[]
    for i in range(count):
        p=base+block.u16(base+6+i*4); raw=block.get(p,len(block.data)-p)
        if b'\0' not in raw: raise ValueError('Unterminated name')
        result.append(raw.split(b'\0',1)[0].decode('ascii'))
    return result


def bti(data):
    b=Block(data);fmt=b.get(0,1)[0];w,h=b.read('HH',2)
    rgba=texture(fmt,w,h,b.get(b.u32(28),len(data)-b.u32(28)))
    return w,h,rgba


def brk_final_colors(data):
    """Final C-register colour keys, as used by Onyon::changeMaterial at rest."""
    if data[:8]!=b'J3D1brk1':raise ValueError('Expected BRK colour animation')
    b=Block(data[32:]);result={};table=b.u32(32);labels=names(b,b.u32(48))
    for i in range(b.u16(12)):
        channels=[]
        for c in range(4):
            count,start,kind=b.read('3H',table+i*28+c*6)
            if not count or kind not in (0,1):raise ValueError('Invalid colour key')
            index=start if count==1 else start+(count-1)*(3 if kind==0 else 4)+1
            channels.append(max(0,min(255,b.read('h',b.u32(56+4*c)+2*index)[0])))
        result[labels[i]]=tuple(channels)
    return result


def convert(data, tint=(255, 255, 255), tint_materials=(), external_textures=None, material_colors=None, unlit=False, skinned=False, bake_materials=False, max_texture_size=1024, joint_poses=None):
    if len(data) > 8*1024*1024 or data[:8] != b'J3D2bmd3':
        raise ValueError('Expected bounded J3D2bmd3')
    source = Block(data)
    if source.u32(8) != len(data) or not 1 <= source.u32(12) <= 32:
        raise ValueError('Invalid BMD header')
    chunks = {}; p = 32
    for _ in range(source.u32(12)):
        tag, size = source.read('4sI', p)
        if size < 8 or tag in chunks:
            raise ValueError('Invalid/duplicate BMD chunk')
        chunks[tag] = Block(source.get(p, size)); p += size
    def chunk(tag):
        if tag not in chunks: raise ValueError('Missing BMD chunk '+repr(tag))
        return chunks[tag]
    evp=chunk(b'EVP1')
    jnt, inf, drw, shp, vtx, mat, tex = (chunk(t) for t in (b'JNT1', b'INF1', b'DRW1', b'SHP1', b'VTX1', b'MAT3', b'TEX1'))
    parents = {}; shape_material = {}; stack = []; context = (-1, -1); last = context
    p = inf.u32(20)
    for _ in range(8192):
        kind, index = inf.read('HH', p); p += 4
        if kind == 0: break
        if kind == 1:
            stack.append(context); context = last
        elif kind == 2:
            if not stack: raise ValueError('Unbalanced BMD hierarchy')
            context = stack.pop(); last = context
        elif kind == 16:
            parents[index] = context[0]; last = (index, context[1])
        elif kind == 17:
            context = (context[0], index); last = context
        elif kind == 18:
            shape_material[index] = context[1]; last = context
        else: raise ValueError('Unknown BMD hierarchy node')
    else: raise ValueError('BMD hierarchy limit exceeded')
    joint_count = jnt.u16(8)
    if not 0 < joint_count <= 256: raise ValueError('BMD joint limit exceeded')
    if joint_poses is not None and len(joint_poses)!=joint_count:raise ValueError('BMD pose joint mismatch')
    worlds = {}; visiting = set()
    def world(index):
        if index in worlds: return worlds[index]
        if index >= joint_count or index in visiting or index not in parents:
            raise ValueError('Invalid joint hierarchy')
        visiting.add(index)
        r = jnt.u32(12)+jnt.u16(jnt.u32(16)+2*index)*64
        sx0, sy0, sz0, rx, ry, rz, tx, ty, tz = jnt.read('3f3h2x3f', r+4)
        if joint_poses is not None:
            v=joint_poses[index];sx0,sy0,sz0=v[0],v[3],v[6];rx,ry,rz=v[1],v[4],v[7];tx,ty,tz=v[2],v[5],v[8]
        sx, sy, sz = (math.sin(a*math.pi/32768) for a in (rx, ry, rz))
        cx, cy, cz = (math.cos(a*math.pi/32768) for a in (rx, ry, rz))
        local = [[cz*cy, sx*cz*sy-cx*sz, cx*cz*sy+sx*sz, tx],
                 [sz*cy, sx*sz*sy+cx*cz, cx*sz*sy-sx*cz, ty],
                 [-sy, cy*sx, cy*cx, tz], [0, 0, 0, 1]]
        for row in range(3):
            for col,scale in enumerate((sx0,sy0,sz0)):local[row][col]*=scale
        parent = parents[index]
        if parent >= 0 and jnt.get(r+2,1)[0] == 1:
            pr=jnt.u32(12)+jnt.u16(jnt.u32(16)+2*parent)*64
            parent_scale=jnt.read('3f',pr+4) if joint_poses is None else [joint_poses[parent][i] for i in (0,3,6)]
            for row,scale in enumerate(parent_scale):
                if abs(scale)<1e-8:raise ValueError('Zero parent scale')
                for col in range(3):local[row][col]/=scale
        worlds[index] = multiply(world(parent), local) if parent >= 0 else local
        visiting.remove(index)
        return worlds[index]
    envelopes=[];cursor=0
    for i in range(evp.u16(8)):
        count=evp.get(evp.u32(12)+i,1)[0]
        if not 1<=count<=32:raise ValueError('Invalid envelope count')
        blended=[[0.0]*4 for _ in range(4)];total=0
        for j in range(count):
            joint=evp.u16(evp.u32(16)+2*(cursor+j));weight=evp.read('f',evp.u32(20)+4*(cursor+j))[0]
            if not math.isfinite(weight) or weight<0:raise ValueError('Invalid envelope weight')
            rows=evp.read('12f',evp.u32(24)+joint*48)
            inv=[list(rows[r*4:r*4+4]) for r in range(3)]+[[0,0,0,1]]
            matrix=multiply(world(joint),inv);total+=weight
            for r in range(4):
                for c in range(4):blended[r][c]+=weight*matrix[r][c]
        if abs(total-1)>.005:raise ValueError('Envelope weights must sum to one')
        envelopes.append(blended);cursor+=count
    draw_matrices = []
    for i in range(drw.u16(8)):
        flag=drw.get(drw.u32(12)+i,1)[0];index=drw.u16(drw.u32(16)+2*i)
        if flag==0:draw_matrices.append(world(index))
        elif flag==1 and index<len(envelopes):draw_matrices.append(envelopes[index])
        else:raise ValueError('Invalid envelope draw index')
    formats = {}; p = vtx.u32(8)
    for _ in range(32):
        attr, count, kind, frac = vtx.read('IIIB3x', p); p += 16
        if attr == 255: break
        formats[attr] = (count, kind, frac)
    else: raise ValueError('Missing vertex format terminator')
    offsets = vtx.read('13I', 12)
    array_map = {9:0, 10:1, 11:3, 12:4, **{13+i:5+i for i in range(8)}}
    def array(attr, index):
        if attr not in formats or attr not in array_map: raise ValueError('Unsupported vertex array')
        count, kind, frac = formats[attr]
        if attr in (11,12):
            if kind!=5:raise ValueError('Only RGBA8 indexed vertex colours supported')
            start=offsets[array_map[attr]];end=min([o for o in offsets if o>start]+[len(vtx.data)])
            if not start or (index+1)*4>end-start:raise ValueError('Vertex colour index out of bounds')
            return tuple(v/255 for v in vtx.get(start+index*4,4))
        n = (3 if count else 2) if attr == 9 else 3 if attr == 10 else (2 if count else 1)
        types = {0:('B',1), 1:('b',1), 2:('H',2), 3:('h',2), 4:('f',4)}
        if kind not in types: raise ValueError('Unsupported vertex component encoding')
        f, size = types[kind]; start = offsets[array_map[attr]]
        end = min([o for o in offsets if o > start]+[len(vtx.data)])
        if not start or (index+1)*n*size > end-start: raise ValueError('Vertex index out of bounds')
        return tuple(x/(2**frac if kind != 4 else 1) for x in vtx.read(str(n)+f, start+index*n*size))
    textures = []
    if tex.u16(8) > 128: raise ValueError('Texture count exceeds limit')
    for i in range(tex.u16(8)):
        h = tex.u32(12)+32*i; fmt = tex.get(h,1)[0]; w, he = tex.read('HH',h+2)
        if external_textures and i in external_textures:
            w,he,rgba=bti(external_textures[i])
        else:rgba = texture(fmt,w,he,tex.get(h+tex.u32(h+28),len(tex.data)-h-tex.u32(h+28)))
        if max(w,he)>max_texture_size:
            ratio=max(w,he)/max_texture_size;nw=max(8,int(w/ratio));nh=max(8,int(he/ratio));nw=1<<(nw-1).bit_length();nh=1<<(nh-1).bit_length()
            rgba=b''.join(rgba[(min(he-1,y*he//nh)*w+min(w-1,x*w//nw))*4:][:4] for y in range(nh) for x in range(nw));w,he=nw,nh
        pw=max(8,1<<(w-1).bit_length());ph=max(8,1<<(he-1).bit_length())
        if (pw,ph)!=(w,he):
            padded=bytearray(pw*ph*4)
            for y in range(ph):
                for x in range(pw):
                    src=(min(y,he-1)*w+min(x,w-1))*4;p=(y*pw+x)*4
                    padded[p:p+4]=rgba[src:src+4]
            rgba=padded
        textures.append((pw,ph,tex.get(h+6,2),tiled_rgba(rgba,pw,ph),w/pw,he/ph))
    bake_domain=(0,1,0,1)
    if bake_materials and 13 in formats:
        points=[]
        for index in range(100000):
            try:points.append(array(13,index))
            except ValueError:break
        if points:
            u0,u1=min(p[0] for p in points),max(p[0] for p in points);v0,v1=min(p[1] for p in points),max(p[1] for p in points)
            if u1-u0>.001 and v1-v0>.001:bake_domain=(u0,u1,v0,v1)
    material_names=names(mat,mat.u32(20))
    batches = []; total_vertices = 0; all_positions = []
    for shape in range(shp.u16(8)):
        if shape not in shape_material: raise ValueError('Shape lacks material')
        material = shape_material[shape]
        if material < 0 or material >= mat.u16(8): raise ValueError('Invalid material index')
        r = mat.u32(12)+mat.u16(mat.u32(16)+2*material)*332
        material_mode=mat.get(r,1)[0]
        color = tuple(v/255 for v in mat.get(mat.u32(32)+mat.u16(r+8)*4,4))
        if material_colors and material_names[material] in material_colors:
            color=tuple(v/255 for v in material_colors[material_names[material]])
        if material in tint_materials: color = tuple(color[i]*(tint[i]/255 if i<3 else 1) for i in range(4))
        ti = mat.u16(r+132)
        texture_id = mat.u16(mat.u32(72)+ti*2) if ti != 65535 else -1
        if texture_id >= len(textures): raise ValueError('Material texture index out of bounds')
        if bake_materials:
            from material_bake import bake
            w,h,pixels=bake(mat,r,tex,(material_colors or {}).get(material_names[material]),bake_domain)
            texture_id=len(textures);textures.append((w,h,bytes((0,0)),tiled_rgba(pixels,w,h),1,1));color=(1,1,1,1)
        r = shp.u32(12)+shp.u16(shp.u32(16)+2*shape)*40
        matrix_type, groups, desc, mtx_init, draw_init = shp.read('Bx4H',r)
        if matrix_type not in (0,3) or groups > 4096: raise ValueError('Unsupported shape matrix type/count')
        attrs = []; p = shp.u32(24)+desc
        for _ in range(24):
            a, t = shp.read('II',p); p += 8
            if a == 255: break
            if not ((0 <= a <= 8 and t == 1) or (a in (9,10,11,12,13,14,15,16,17,18,19,20) and t in (2,3))):
                raise ValueError('Unsupported display-list vertex descriptor')
            attrs.append((a,t))
        else: raise ValueError('Missing vertex descriptor terminator')
        vertices = []; previous = []
        for packet in range(groups):
            use, count, first = shp.read('HHI',shp.u32(36)+(mtx_init+packet)*8)
            if matrix_type == 0: palette = [use]
            else:
                palette = list(shp.read(str(count)+'H',shp.u32(28)+first*2))
                for i,x in enumerate(palette):
                    if x == 65535:
                        if i >= len(previous): raise ValueError('Unresolved matrix palette reuse')
                        palette[i] = previous[i]
            previous = palette
            length, offset = shp.read('II',shp.u32(40)+(draw_init+packet)*8)
            dl = Block(shp.get(shp.u32(32)+offset,length)); p = 0
            while p < length:
                opcode = dl.get(p,1)[0]; p += 1
                if opcode == 0: continue
                if opcode not in (0x80,0x90,0x98,0xa0): raise ValueError('Unsupported GX display-list command')
                count = dl.u16(p); p += 2; points = []
                for _ in range(count):
                    indices = {}
                    for attr,t in attrs:
                        indices[attr] = dl.u16(p) if t == 3 else dl.get(p,1)[0]
                        p += 2 if t == 3 else 1
                    slot = indices.get(0,0)//3
                    if slot >= len(palette) or palette[slot] >= len(draw_matrices): raise ValueError('Invalid draw matrix slot')
                    matrix = draw_matrices[palette[slot]]
                    pos = array(9,indices[9]) if skinned else transform(matrix,array(9,indices[9]))
                    normal = transform(matrix,array(10,indices[10]),0) if 10 in indices else (0,1,0)
                    light = 1 if unlit else .55+.45*max(0,sum(a*b for a,b in zip(normal,(.3,.85,.43))))
                    uv = array(13,indices[13]) if 13 in indices else (0,0)
                    if bake_materials:uv=((uv[0]-bake_domain[0])/(bake_domain[1]-bake_domain[0]),(uv[1]-bake_domain[2])/(bake_domain[3]-bake_domain[2]))
                    vertex_color=array(11,indices[11]) if 11 in indices else (1,1,1,1)
                    # Native sampled texture is flipped vertically in memory.
                    us,vs=textures[texture_id][4:] if texture_id>=0 else (1,1)
                    bone=(palette[slot]+1)*3 if skinned else 0
                    if bone>87:raise ValueError('Animated palette exceeds PICA uniform budget')
                    vertex = (*pos,1,*(color[i]*light*vertex_color[i] for i in range(3)),color[3]*vertex_color[3],uv[0]*us,1-uv[1]*vs,bone,0)
                    points.append(vertex)
                if opcode == 0x90:
                    if count%3: raise ValueError('Incomplete triangle list')
                    triangles = [points[i:i+3] for i in range(0,count,3)]
                elif opcode == 0x80:
                    if count%4: raise ValueError('Incomplete quad list')
                    triangles = [[points[i],points[i+1],points[i+2]] for i in range(0,count,4)]+[[points[i],points[i+2],points[i+3]] for i in range(0,count,4)]
                elif opcode == 0x98:
                    triangles = [[points[i+(i%2)],points[i+1-(i%2)],points[i+2]] for i in range(count-2)]
                else: triangles = [[points[0],points[i],points[i+1]] for i in range(1,count-1)]
                for tri in triangles:
                    vertices.extend(tri)
                    all_positions.extend(v[:3] for v in tri)
                if len(vertices) > 150000: raise ValueError('Triangle expansion limit exceeded')
        batches.append((texture_id,vertices,material_mode)); total_vertices += len(vertices)
    if not all_positions or total_vertices > 200000: raise ValueError('Empty/oversize model')
    # Simple checked, little-endian runtime container; no raw J3D pointer traversal.
    payload = bytearray()
    for w,h,wrap,pixels,_,_ in textures:
        payload.extend(struct.pack('<4I',w,h,wrap[0],wrap[1])); payload.extend(pixels)
    for texture_id,vertices,mode in batches:
        payload.extend(struct.pack('<iII',texture_id,len(vertices),mode))
        for vertex in vertices: payload.extend(struct.pack('<12f',*vertex))
    result = struct.pack('<4s5I',b'P2M2',len(payload),zlib.crc32(payload),len(textures),len(batches),total_vertices)+payload
    bounds = [[min(p[i] for p in all_positions),max(p[i] for p in all_positions)] for i in range(3)]
    return bytes(result), {'vertices':total_vertices,'triangles':total_vertices//3,'textures':len(textures),'batches':len(batches),'bounds':bounds,
                           'pose':'bind pose; rigid and weighted matrices','materials':'base colour / first texture; approximate TEV'}
