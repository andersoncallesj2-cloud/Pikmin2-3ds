"""Convert selected original ground-HUD BTIs; native demo layout, not J2D BLO."""
import struct
import zlib
from j3d_model import bti, tiled_rgba

HUD_NAMES=[f'{i}_32.bti' for i in range(10)]+['s_32.bti','rp_l64.bti','orima_pk2.bti','loozy_pk2.bti','orima_base.bti','coin_32.bti']


def convert_hud(archive):
    payload=bytearray();metadata=[]
    for name in HUD_NAMES:
        w,h,pixels=bti(archive['timg/'+name])
        pw=max(8,1<<(w-1).bit_length());ph=max(8,1<<(h-1).bit_length())
        padded=bytearray(pw*ph*4)
        for y in range(h):padded[y*pw*4:y*pw*4+w*4]=pixels[y*w*4:(y+1)*w*4]
        payload.extend(struct.pack('<4I',pw,ph,w,h));payload.extend(tiled_rgba(padded,pw,ph))
        metadata.append({'source':name,'width':w,'height':h})
    return struct.pack('<4s3I',b'P2U1',len(payload),zlib.crc32(payload),len(HUD_NAMES))+payload,metadata
