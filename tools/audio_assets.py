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
       'AudioRes/Stream/ff_playerdown.w.32.c4.ast','AudioRes/Stream/pikmin_greet_new.h.32.c4.ast',
       'AudioRes/PSound.aaf','AudioRes/Banks/wScene_0.aw','AudioRes/Seqs/Seq.arc']
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
    import wave
    data=(romfs/'pikmin-greeting.p2s').read_bytes();rate=struct.unpack_from('<I',data,4)[0]
    with wave.open(str(romfs.parent.parent/'assets/menu.wav'),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(rate);w.writeframes(data[24:])
    build_effects(resources,romfs,report)
    from event_audio import build as events
    events(resources,romfs,report,PATHS,COEFS)

def build_effects(resources,romfs,report):
    from game_archive import rarc
    a=Block(resources[PATHS[4]]);p=0;banks={};ws=None
    while True:
        section=a.u32(p);p+=4
        if not section:break
        if section in (2,3):
            while a.u32(p):
                off,size,_=a.read('3I',p);p+=12;block=Block(a.get(off,size))
                if section==2:banks[block.u32(8)]=block
                else:ws=block
            p+=4
        else:p+=12
    if ws is None:raise ValueError('Original WSYS missing')
    seq=rarc(resources[PATHS[6]])['se.bms'];wavemap={};wa,cg=ws.read('2I',16)
    for group in range(ws.u32(cg+8)):
        arc=ws.u32(wa+8+group*4);ctrl=ws.u32(ws.u32(cg+12+group*4)+12)
        filename=ws.get(arc,112).split(b'\0')[0].decode('ascii')
        for j in range(ws.u32(ctrl+4)):
            wid=ws.u32(ws.u32(ctrl+8+j*4))&65535;wavemap[wid]=(filename,ws.u32(arc+116+j*4))
    # Verified original se.bms dispatch: PSSE_PL_THROW 0806 -> 00e5/note1;
    # PSSE_SY_CHANGE_ORIMA 188d -> 021b/note60; CHANGE_LUI 188e -> 021c.
    for name,sid,bank,program,note in [('throw',0x806,0,229,1),('switch-olimar',0x188d,2,27,60),('switch-louie',0x188e,2,28,60)]:
        token=b'\xac\x06'+struct.pack('>H',bank*256+program)
        matches=[i for i in range(len(seq)-4) if seq[i:i+4]==token]
        if not any(bytes((note,8,127)) in seq[i:i+32] for i in matches):raise ValueError('Original sound dispatch not recognized: '+name)
        b=banks[bank]
        if program<128:
            inst=b.u32(36+program*4);key=next(b.u32(inst+44+i*4) for i in range(b.u32(inst+40)) if b.get(b.u32(inst+44+i*4),1)[0]>=note);vmap=b.u32(key+8)
        else:
            perc=b.u32(0x3b4+(program-228)*4);mapping=b.u32(perc+0x88+note*4);vmap=b.u32(mapping+20)
        wid=b.u32(vmap+4)&65535;filename,w=wavemap[wid]
        if filename!='wScene_0.aw' or ws.get(w+1,1)[0]!=0:raise ValueError('Unsupported original sound wave')
        rate,offset,size=ws.read('f2I',w+4);n=ws.u32(w+28);raw=resources[PATHS[5]][offset:offset+size];samples=array.array('h');h1=h2=0
        for frame in range(0,len(raw)//9*9,9):
            head=raw[frame];c1,c2=COEFS[head&15]
            for byte in raw[frame+1:frame+9]:
                for value in (byte>>4,byte&15):
                    if value>=8:value-=16
                    value=max(-32768,min(32767,((value*(1<<(head>>4))<<11)+c1*h1+c2*h2)>>11));samples.append(value);h2,h1=h1,value
        samples=samples[:n];pcm=array.array('h',[0])*(n*2);pcm[0::2]=pcm[1::2]=samples
        if sys.byteorder!='little':pcm.byteswap()
        (romfs/(name+'.p2s')).write_bytes(struct.pack('<4s5I',b'P2S1',round(rate),2,n,0,0)+pcm.tobytes())
        report['audio']['streams'][name]={'sound_id':hex(sid),'bank':bank,'program':program,'wave_id':wid,'source':'AudioRes/Banks/'+filename,'rate':rate,'frames':n}
