"""Private HOME Menu artwork derived from opening.bnr and a disc Pikmin icon."""
import struct
import zlib
from disc_asset import decode_banner
from j3d_model import bti


def png(path,w,h,pixels):
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
    raw=b''.join(b'\0'+pixels[y*w*4:(y+1)*w*4] for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))


def resize(pixels,w,h,nw,nh):
    return b''.join(pixels[(min(h-1,y*h//nh)*w+min(w-1,x*w//nw))*4:][:4] for y in range(nh) for x in range(nw))


def create(banner,pikmin_icon,app,assets):
    rgb=decode_banner(banner);rgba=b''.join(rgb[i:i+3]+b'\xff' for i in range(0,len(rgb),3))
    # Keep the original aspect ratio; the surrounding matte is newly authored.
    logo=resize(rgba,96,32,240,80);canvas=bytearray(bytes((13,27,14,255))*256*128)
    for y in range(80):canvas[((y+24)*256+8)*4:((y+24)*256+248)*4]=logo[y*240*4:(y+1)*240*4]
    png(assets/'disc-banner.png',256,128,canvas)
    w,h,icon=bti(pikmin_icon);icon=resize(icon,w,h,48,48)
    # SMDH stores opaque RGB565. Composite original alpha against a dark matte.
    out=bytearray(icon)
    for i in range(0,len(out),4):
        a=out[i+3]
        for c,bg in enumerate((13,27,14)):out[i+c]=(out[i+c]*a+bg*(255-a))//255
        out[i+3]=255
    png(app/'icon.png',48,48,out)
