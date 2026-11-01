"""Resolve selected original SE dispatches to bank/wave samples.

Bounded BMS subset for first-note sample extraction, not a JAudio sequencer.
Unsupported commands are reported, never replaced with unrelated samples.
"""
import array,struct,sys
from j3d_model import Block
from game_archive import rarc
SOUNDS=[0x7,0x9f,0xd,0xe,0x802,0x805,0x806,0x808,0x8a1,0x811,0x822,0x842,0x828,0x848,
        0x180f,0x1888,0x1008,0x1009,0x188d,0x188e,0x2802,0x2803,0x2804,0x2806,0x2808,0x280d,0x280c,
        0x2812,0x2815,0x281d,0x281e,0x281f,0x2816,0x2817,0x284c,0x3833]+list(range(0x5811,0x5822))


def march_phrase(seq,entry,path,base_key):
    """Render the first indexed march phrase, not the full interactive sequence.

    JASSeqParser::cmdNoteOn supplies key, velocity, gate percent and tick count.
    The two sample instruments use native resampling and short release ramps.
    Port handshakes/calls are skipped; later dynamically selected phrases remain
    outside this adapter. No unrelated melody or synthesized voice is used.
    """
    jump=seq.find(b'\xc8\xc0',entry,entry+128)
    if jump<0:raise ValueError('March phrase table missing')
    table=int.from_bytes(seq[jump+3:jump+6],'big');p=int.from_bytes(seq[table:table+3],'big');notes=[];ticks=0
    for _ in range(1024):
        op=seq[p];p+=1
        if op<128:
            flag,vel,gate=seq[p:p+3];p+=3
            if flag!=8:break
            delay=seq[p];p+=1;notes.append((op,vel,gate,ticks,delay));ticks+=delay
        elif op==0x80:ticks+=seq[p];p+=1
        elif op==0x88:ticks+=int.from_bytes(seq[p:p+2],'big');p+=2
        elif op==0xa4:p+=2
        elif op==0xc4:p+=4
        else:break
        if ticks>1200:raise ValueError('March phrase duration limit')
    if len(notes)<8 or not ticks:raise ValueError('March phrase is incomplete')
    raw=path.read_bytes();h=struct.unpack_from('<4s5I',raw);source=array.array('h');source.frombytes(raw[24:])
    if sys.byteorder!='little':source.byteswap()
    mono=source[::2];rate=32000;seconds_per_tick=60/(120*48);n=round(ticks*seconds_per_tick*rate);out=[0.]*n
    for key,vel,gate,start,delay in notes:
        first=round(start*seconds_per_tick*rate);length=round(delay*seconds_per_tick*rate*gate/100);step=h[1]/rate*2**((key-base_key)/12);gain=(vel/110)**2
        for i in range(min(length,n-first)):
            x=i*step;k=int(x)
            if k+1>=len(mono):break
            sample=mono[k]+(mono[k+1]-mono[k])*(x-k);ramp=min(1,i/80,(length-i)/120)
            out[first+i]+=sample*gain*ramp
    mono=array.array('h',(max(-32768,min(32767,round(v))) for v in out));pcm=array.array('h',[0])*(n*2);pcm[::2]=pcm[1::2]=mono
    if sys.byteorder!='little':pcm.byteswap()
    path.write_bytes(struct.pack('<4s5I',b'P2S1',rate,2,n,0,n)+pcm.tobytes())
    return {'notes':len(notes),'ticks':ticks,'tempo':120,'timebase':48,'subset':'first original BMS march phrase; native pitch/envelope adapter, not complete JAudio sequencing'}

def first_note(seq,entry,regs=None,depth=0):
    if depth>12:return None
    regs={} if regs is None else regs;p=entry;visited=set()
    for _ in range(256):
        if p>=len(seq) or p in visited:return None
        visited.add(p);op=seq[p];p+=1
        if op<128:
            flag,vel=seq[p:p+2]
            return regs.get(6,0),regs.get(op,op) if flag&128 else op,vel&127
        if op in (0xa4,0xac,0xa5,0xad):
            reg=seq[p];p+=1;n=1 if op in (0xa4,0xa5) else 2;v=int.from_bytes(seq[p:p+n],'big');p+=n
            if reg==0x20:regs[6]=(v<<8)|(regs.get(6,0)&255)
            elif reg==0x21:regs[6]=(regs.get(6,0)&0xff00)|v
            else:regs[reg]=regs.get(reg,0)+v if op in (0xa5,0xad) else v
        elif op==0xa9:
            mode=seq[p];dest=seq[p+1];n=2 if mode&12==12 else 1;regs[dest]=0;p+=2+n
        elif op==0xd8:p+=10
        elif op==0xd9:p+=1
        elif op==0xe7:p+=2
        elif op==0xc1:
            target=int.from_bytes(seq[p+1:p+4],'big');p+=4
            result=first_note(seq,target,dict(regs),depth+1)
            if result:return result
        elif op==0xc4:
            flag=seq[p];p+=1
            if flag&128:return None
            target=int.from_bytes(seq[p:p+3],'big');p+=3
            if flag==0:
                result=first_note(seq,target,regs,depth+1)
                if result:return result
        elif op in (0xc6,0xff):return None
        elif op in (0x80,0x88):p+=1 if op==0x80 else 2
        elif op==0xc8:
            flag=seq[p];p+=1
            if flag&128:
                reg=seq[p];p+=1
                if flag&64:
                    table=int.from_bytes(seq[p:p+3],'big');p+=3;target=int.from_bytes(seq[table+regs.get(reg,0)*3:table+regs.get(reg,0)*3+3],'big');p=target
                else:return None
            else:
                target=int.from_bytes(seq[p:p+3],'big');p+=3
                if flag==0:p=target
        elif 0x90<=op<=0x9f:
            size=(op&12);p+=1+(2 if size==12 else 1)+(0,1,1,2)[op&3]
        elif op in (0xef,):p+=3
        elif op in (0xdb,0xdd):p+=3 if op==0xdd else 1
        elif op in (0xf0,0xc9):p+=1 if op==0xc9 else 1
        elif 0x81<=op<=0x87:pass
        elif op==0xda:p+=1
        else:return None
    return None

def build(resources,romfs,report,paths,coefs):
    a=Block(resources[paths[4]]);p=0;banks={};ws=None
    while a.u32(p):
        section=a.u32(p);p+=4
        if section in (2,3):
            while a.u32(p):
                off,size,_=a.read('3I',p);p+=12;b=Block(a.get(off,size))
                if section==2:banks[b.u32(8)]=b
                else:ws=b
            p+=4
        else:p+=12
    if ws is None:raise ValueError('Missing WSYS')
    seq=rarc(resources[paths[6]])['se.bms'];wavemap={};wa,cg=ws.read('2I',16)
    for group in range(ws.u32(cg+8)):
        arc=ws.u32(wa+8+group*4);ctrl=ws.u32(ws.u32(cg+12+group*4)+12);filename=ws.get(arc,112).split(b'\0')[0].decode('ascii')
        for j in range(ws.u32(ctrl+4)):wavemap[ws.u32(ws.u32(ctrl+8+j*4))&65535]=(filename,ws.u32(arc+116+j*4))
    tables={}
    for category,start in enumerate((0x2411,0x39c,0x8518,0x45d1,0x10cd7,0xb1b2)):
        at=seq.find(bytes.fromhex("c4c004"),start,start+300)
        if at<0:raise ValueError("Original SE dispatch table missing")
        tables[category]=int.from_bytes(seq[at+3:at+6],"big");resolved={};unresolved={}
    for sid in SOUNDS:
        try:
            table=tables[sid>>12];entry=int.from_bytes(seq[table+(sid&1023)*3:table+(sid&1023)*3+3],'big');note=first_note(seq,entry)
            if note is None:raise ValueError('BMS first-note path requires additional sequence commands')
            bankprog,key,velocity=note;bank,prog=bankprog>>8,bankprog&255;b=banks[bank];pitch=1.;volume=1.
            if prog<128:
                inst=b.u32(36+prog*4);volume,pitch=b.read('2f',inst+8)
                region=next(b.u32(inst+44+i*4) for i in range(b.u32(inst+40)) if b.get(b.u32(inst+44+i*4),1)[0]>=key)
                vmap=next(b.u32(region+8+i*4) for i in range(b.u32(region+4)) if b.get(b.u32(region+8+i*4),1)[0]>=velocity)
            else:
                perc=b.u32(0x3b4+(prog-228)*4);mapping=b.u32(perc+0x88+key*4);pitch,volume=b.read('2f',mapping)
                vmap=next(b.u32(mapping+20+i*4) for i in range(b.u32(mapping+16)) if b.get(b.u32(mapping+20+i*4),1)[0]>=velocity)
            wid=b.u32(vmap+4)&65535;vg,pg=b.read('2f',vmap+8);volume*=vg;pitch*=pg;filename,w=wavemap[wid]
            if filename!='wScene_0.aw' or ws.get(w+1,1)[0]!=0:raise ValueError('Sample requires another wave archive/codec')
            rate,off,size=ws.read('f2I',w+4);n=ws.u32(w+28)
            if prog<128:pitch*=2**((key-ws.get(w+2,1)[0])/12)
            raw=resources[paths[5]][off:off+size];samples=array.array('h');h1=h2=0
            for frame in range(0,len(raw)//9*9,9):
                head=raw[frame];c1,c2=coefs[head&15]
                for byte in raw[frame+1:frame+9]:
                    for v in (byte>>4,byte&15):
                        if v>=8:v-=16
                        v=max(-32768,min(32767,((v*(1<<(head>>4))<<11)+c1*h1+c2*h2)>>11));samples.append(v);h2,h1=h1,v
            samples=samples[:n];gain=min(1,volume*(velocity/127)**2);samples=array.array('h',(round(v*gain) for v in samples));pcm=array.array('h',[0])*(len(samples)*2);pcm[::2]=pcm[1::2]=samples
            if sys.byteorder!='little':pcm.byteswap()
            rate=max(8000,min(48000,round(rate*pitch)));(romfs/f'sfx-{sid:04x}.p2s').write_bytes(struct.pack('<4s5I',b'P2S1',rate,2,len(samples),0,0)+pcm.tobytes());resolved[hex(sid)]={'wave':wid,'bank':bank,'program':prog,'key':key,'rate':rate}
        except (ValueError,KeyError,IndexError,StopIteration,struct.error) as exc:unresolved[hex(sid)]=str(exc)
    phrases={}
    for sid in (0xd,0xe):
        if hex(sid) in resolved:
            entry=int.from_bytes(seq[tables[0]+sid*3:tables[0]+sid*3+3],'big')
            try:phrases[hex(sid)]=march_phrase(seq,entry,romfs/f'sfx-{sid:04x}.p2s',resolved[hex(sid)]['key'])
            except (ValueError,IndexError,struct.error) as exc:unresolved['march-'+hex(sid)]=str(exc)
    report['audio']['march_phrases']=phrases
    report['audio']['event_samples']=resolved;report['audio']['unresolved_events']=unresolved
