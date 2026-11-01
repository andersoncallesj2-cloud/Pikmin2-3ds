"""Bounded rigid-bind-pose BMD -> native textured triangle batches.

New converter; format references are projectPiki J3DFileBlock/J3DJointFactory,
J3DShapeFactory and J3DGetTranslateRotateMtx. No original assets in this file.
This deliberately rejects weighted skinning and unsupported GX vertex formats.
Materials approximate base colour/first texture, not the full GX TEV pipeline.
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
    """3DS RGBA8: 8x8 Morton tiles, ABGR bytes, vertical orientation for UVs."""
    if width < 8 or height < 8 or width & (width-1) or height & (height-1):
        raise ValueError('Native textures require power-of-two dimensions >=8')
    out = bytearray(len(pixels))
    for y in range(height):
        for x in range(width):
            morton = sum(((x >> b & 1) << (2*b)) | ((y >> b & 1) << (2*b+1)) for b in range(3))
            dst = ((y//8)*(width//8)*64+(x//8)*64+morton)*4
            src = ((height-1-y)*width+x)*4
            out[dst:dst+4] = pixels[src:src+4][::-1]
    return bytes(out)


def multiply(a, b):
    return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def transform(m, v, w=1):
    return tuple(sum(m[i][j]*v[j] for j in range(3))+m[i][3]*w for i in range(3))


def convert(data, tint=(255, 255, 255), tint_materials=()):
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
    if chunk(b'EVP1').u16(8):
        raise ValueError('Weighted envelopes not supported in this increment')
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
    worlds = {}; visiting = set()
    def world(index):
        if index in worlds: return worlds[index]
        if index >= joint_count or index in visiting or index not in parents:
            raise ValueError('Invalid joint hierarchy')
        visiting.add(index)
        r = jnt.u32(12)+jnt.u16(jnt.u32(16)+2*index)*64
        sx0, sy0, sz0, rx, ry, rz, tx, ty, tz = jnt.read('3f3h2x3f', r+4)
        if any(abs(s-1) > 1e-5 for s in (sx0, sy0, sz0)):
            raise ValueError('Non-unit joint scale requires scale-compensation support')
        sx, sy, sz = (math.sin(a*math.pi/32768) for a in (rx, ry, rz))
        cx, cy, cz = (math.cos(a*math.pi/32768) for a in (rx, ry, rz))
        local = [[cz*cy, sx*cz*sy-cx*sz, cx*cz*sy+sx*sz, tx],
                 [sz*cy, sx*sz*sy+cx*cz, cx*sz*sy-sx*cz, ty],
                 [-sy, cy*sx, cy*cx, tz], [0, 0, 0, 1]]
        parent = parents[index]
        worlds[index] = multiply(world(parent), local) if parent >= 0 else local
        visiting.remove(index)
        return worlds[index]
    draw_matrices = []
    for i in range(drw.u16(8)):
        if drw.get(drw.u32(12)+i, 1) != b'\0': raise ValueError('Weighted draw matrix not supported')
        draw_matrices.append(world(drw.u16(drw.u32(16)+2*i)))
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
        n = (3 if count else 2) if attr == 9 else 3 if attr == 10 else (2 if count else 1)
        types = {0:('B',1), 1:('b',1), 2:('H',2), 3:('h',2), 4:('f',4)}
        if kind not in types: raise ValueError('Unsupported vertex component encoding')
        f, size = types[kind]; start = offsets[array_map[attr]]
        end = min([o for o in offsets if o > start]+[len(vtx.data)])
        if not start or (index+1)*n*size > end-start: raise ValueError('Vertex index out of bounds')
        return tuple(x/(2**frac if kind != 4 else 1) for x in vtx.read(str(n)+f, start+index*n*size))
    textures = []
    if tex.u16(8) > 32: raise ValueError('Texture count exceeds limit')
    for i in range(tex.u16(8)):
        h = tex.u32(12)+32*i; fmt = tex.get(h,1)[0]; w, he = tex.read('HH',h+2)
        rgba = texture(fmt,w,he,tex.get(h+tex.u32(h+28),len(tex.data)-h-tex.u32(h+28)))
        textures.append((w,he,tex.get(h+6,2),tiled_rgba(rgba,w,he)))
    batches = []; total_vertices = 0; all_positions = []
    for shape in range(shp.u16(8)):
        if shape not in shape_material: raise ValueError('Shape lacks material')
        material = shape_material[shape]
        if material < 0 or material >= mat.u16(8): raise ValueError('Invalid material index')
        r = mat.u32(12)+mat.u16(mat.u32(16)+2*material)*332
        color = tuple(v/255 for v in mat.get(mat.u32(32)+mat.u16(r+8)*4,4))
        if material in tint_materials: color = tuple(color[i]*(tint[i]/255 if i<3 else 1) for i in range(4))
        ti = mat.u16(r+132)
        texture_id = mat.u16(mat.u32(72)+ti*2) if ti != 65535 else -1
        if texture_id >= len(textures): raise ValueError('Material texture index out of bounds')
        r = shp.u32(12)+shp.u16(shp.u32(16)+2*shape)*40
        matrix_type, groups, desc, mtx_init, draw_init = shp.read('Bx4H',r)
        if matrix_type not in (0,3) or groups > 4096: raise ValueError('Unsupported shape matrix type/count')
        attrs = []; p = shp.u32(24)+desc
        for _ in range(24):
            a, t = shp.read('II',p); p += 8
            if a == 255: break
            if not ((a == 0 and t == 1) or (a in (9,10,13) and t in (2,3))):
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
                    pos = transform(matrix,array(9,indices[9]))
                    normal = transform(matrix,array(10,indices[10]),0) if 10 in indices else (0,1,0)
                    light = .55+.45*max(0,sum(a*b for a,b in zip(normal,(.3,.85,.43))))
                    uv = array(13,indices[13]) if 13 in indices else (0,0)
                    # Native sampled texture is flipped vertically in memory.
                    vertex = (*pos,1,*(color[i]*light for i in range(3)),color[3],uv[0],1-uv[1],0,0)
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
        batches.append((texture_id,vertices)); total_vertices += len(vertices)
    if not all_positions or total_vertices > 200000: raise ValueError('Empty/oversize model')
    # Simple checked, little-endian runtime container; no raw J3D pointer traversal.
    payload = bytearray()
    for w,h,wrap,pixels in textures:
        payload.extend(struct.pack('<4I',w,h,wrap[0],wrap[1])); payload.extend(pixels)
    for texture_id,vertices in batches:
        payload.extend(struct.pack('<iI',texture_id,len(vertices)))
        for vertex in vertices: payload.extend(struct.pack('<12f',*vertex))
    result = struct.pack('<4s5I',b'P2M1',len(payload),zlib.crc32(payload),len(textures),len(batches),total_vertices)+payload
    bounds = [[min(p[i] for p in all_positions),max(p[i] for p in all_positions)] for i in range(3)]
    return bytes(result), {'vertices':total_vertices,'triangles':total_vertices//3,'textures':len(textures),'batches':len(batches),'bounds':bounds,
                           'pose':'rigid bind pose','materials':'base colour / first texture; approximate TEV'}
