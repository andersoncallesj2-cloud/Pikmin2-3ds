"""Private AST/AFC streams -> PCM for native NDSP streaming.

New bounded decoder, format cross-checked against vgmstream's primary AST/AFC
implementation. No third-party decoder source is embedded. This is not JAudio:
sequenced area BGM, BMS sound effects, spatialisation and voice routing remain
separate work. The demo uses the disc's title stream as its music.
"""
import array
import struct
import sys
from j3d_model import Block

PATHS=['AudioRes/Stream/title.w.32.c4.ast','AudioRes/Stream/ff_treasureget.w.32.c4.ast',
       'AudioRes/Stream/ff_playerdown.w.32.c4.ast','AudioRes/Stream/pikmin_greet_new.h.32.c4.ast']
COEFS=((0,0),(2048,0),(0,2048),(1024,1024),(4096,-2048),(3584,-1536),(3072,-1024),
       (4608,-2560),(4200,-2248),(4800,-2300),(5120,-3072),(2048,-2048),(1024,-1024),
       (-1024,1024),(-1024,0),(-2048,0))


def decode(data):
    b=Block(data)
    codec=b.u16(8)
    if b.get(0,4)!=b'STRM' or codec not in (0,1) or b.u16(10)!=16:raise ValueError('Unsupported AST codec')
    channels=b.u16(12);rate,n,start,end=b.read('4I',16)
    if channels not in (1,2) or not 8000<=rate<=48000 or not 0<n<=rate*300 or end>n:raise ValueError('Invalid AST extent')
    loop=b.u16(14)!=0;hist=[[0,0] for _ in range(channels)];samples=[array.array('h') for _ in range(channels)];p=64
    while len(samples[0])<n:
        if b.get(p,4)!=b'BLCK':raise ValueError('AST block signature')
        size=b.u32(p+4);p+=32
        if not size or size>65536:raise ValueError('AST block size')
        for c in range(channels):
            raw=b.get(p,size);p+=size;h1,h2=hist[c];out=samples[c]
            if codec==1:
                pcm_block=array.array('h');pcm_block.frombytes(raw[:size//2*2])
                if sys.byteorder=='little':pcm_block.byteswap()
                out.extend(pcm_block);continue
            # The final BLCK is padded to 32 bytes, rather than AFC frame size.
            for frame in range(0,size//9*9,9):
                head=raw[frame];a,z=COEFS[head&15];shift=head>>4
                for byte in raw[frame+1:frame+9]:
                    for value in (byte>>4,byte&15):
                        if value>=8:value-=16
                        value=max(-32768,min(32767,((value*(1<<shift)<<11)+a*h1+z*h2)>>11))
                        out.append(value);h2,h1=h1,value
            hist[c]=[h1,h2]
    pcm=array.array('h',[0])*(n*2)
    for c in range(2):pcm[c::2]=samples[c%channels][:n]
    if sys.byteorder!='little':pcm.byteswap()
    return struct.pack('<4s5I',b'P2S1',rate,2,n,start if loop else 0,end if loop else 0)+pcm.tobytes(),{'rate':rate,'frames':n,'loop':loop,'loop_start':start,'loop_end':end}


def build(resources,romfs,report):
    report['audio']={'backend':'native NDSP PCM streaming','music':'original title stream; sequenced area music not ported','streams':{}}
    for name,path in zip(('music','treasure-jingle','captain-down','pikmin-greeting'),PATHS):
        data,info=decode(resources[path]);(romfs/(name+'.p2s')).write_bytes(data)
        report['audio']['streams'][name]={'source':path,**info}
