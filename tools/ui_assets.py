"""Convert original ground HUD textures, BLO2 picture geometry and carry atlas."""
import struct
import zlib
from j3d_model import bti, tiled_rgba
from blo_layout import read_layout

HUD_NAMES=[f'{i}_32.bti' for i in range(10)]+['s_32.bti','rp_l64.bti','orima_pk2.bti','loozy_pk2.bti','orima_base.bti','coin_32.bti']


def convert_hud(archive,game_textures,font_archive=None,menu_archive=None,banner=None,title_archive=None,title_output=None):
    payload=bytearray();metadata=[];indices={};header=[];layouts={}
    def image(name,black=(0,0,0,0),white=(255,255,255,255),raw=None):
        key=(name,black,white)
        if key in indices:return indices[key]
        w,h,pixels=raw if raw is not None else bti(archive['timg/'+name])
        pixels=bytearray(pixels)
        if black!=(0,0,0,0) or white!=(255,255,255,255):
            for i in range(len(pixels)):pixels[i]=max(0,min(255,round(black[i%4]+pixels[i]*(white[i%4]-black[i%4])/255)))
        pw=max(8,1<<(w-1).bit_length());ph=max(8,1<<(h-1).bit_length())
        padded=bytearray(pw*ph*4)
        for y in range(h):padded[y*pw*4:y*pw*4+w*4]=pixels[y*w*4:(y+1)*w*4]
        payload.extend(struct.pack('<4I',pw,ph,w,h));payload.extend(tiled_rgba(padded,pw,ph))
        idx=len(metadata);indices[key]=idx;metadata.append({'source':name,'width':w,'height':h,'black':black,'white':white});return idx
    for name in HUD_NAMES:image(name)
    header.append('static const unsigned hud_piki_images[5]={'+','.join(str(image(n)) for n in ('blp_l64.bti','rp_l64.bti','yp_l64.bti','bp_l64.bti','wp_l64.bti'))+'};')
    header.append('#define HUD_EMPTY_PIKI '+str(image('toumei_piki.bti')))
    for group in ('orima','gr_pikmin','sun_meter'):
        pics,_=read_layout(archive['scrn/'+group+'.blo']);layouts[group]=pics
        header.append('static const HudPane layout_'+group+'[]={')
        for pic in pics:
            idx=image(pic['texture'],pic['black'],pic['white']);points=pic['points']
            x,y=points[0];w=points[1][0]-x;h=points[2][1]-y
            if any(abs(v-e)>.01 for v,e in zip(pic['uv'],(0,0,1,0,0,1,1,1))):raise ValueError('Unsupported HUD texture crop')
            if abs(points[1][1]-y)>.01 or abs(points[2][0]-x)>.01:raise ValueError('Unsupported rotated HUD picture')
            colors=[]
            for corner in pic['colors']:
                colors.extend(c*pic['material_color'][i]/(255*255) for i,c in enumerate(corner))
            header.append('{"'+pic['tag']+'",'+','.join([str(idx),*(format(v,'.8g')+'f' if '.' in format(v,'.8g') else format(v,'.8g')+'.0f' for v in (x,y,w,h))])+',{'+','.join(f'{v:.8f}f' for v in colors)+'}},')
        header.append('};')
    # The world-space counter uses a separate twelve-cell font, not SujiTex32.
    w,h,pixels=bti(game_textures['item_0_9.bti'])
    if w%12:raise ValueError('Invalid original carry atlas')
    cw=w//12;carry_base=len(metadata)
    for glyph in range(12):
        crop=b''.join(pixels[(y*w+glyph*cw)*4:(y*w+(glyph+1)*cw)*4] for y in range(h))
        image('carry_'+str(glyph),raw=(cw,h,crop))
    header.append('#define HUD_CARRY_BASE '+str(carry_base))
    if font_archive:
        from j3d_model import Block,texture
        b=Block(font_archive['pikmin2main.bfn']);g=Block(b.data[64:]);cw,ch=g.read('2H',12);size=g.u32(16);fmt,cols,rows,sw,sh=g.read('5H',20)
        decoded={};base=len(metadata);adv=[];left=[]
        wp=64+g.u32(4)
        while b.get(wp,4)!=b'WID1':wp+=b.u32(wp+4)
        for code in range(32,128):
            glyph=code-32;page=glyph//(cols*rows);cell=glyph%(cols*rows)
            if page not in decoded:decoded[page]=texture(fmt,sw,sh,g.get(32+page*size,size))
            x=(cell%cols)*(cw+1);y=(cell//cols)*(ch+1);pixels=decoded[page];crop=b''.join(pixels[((y+row)*sw+x)*4:((y+row)*sw+x+cw)*4] for row in range(ch))
            image('font_'+str(code),raw=(cw,ch,crop));l,w=b.get(wp+12+glyph*2,2);adv.append(w);left.append(l)
        header.extend(['#define HUD_FONT_BASE '+str(base),'static const unsigned char font_advance[96]={'+','.join(map(str,adv))+'};','static const unsigned char font_left[96]={'+','.join(map(str,left))+'};'])
    if menu_archive:
        for corner in ('lu','ru','ll','rl'):
            name='new_mg_window_'+corner+'.bti';header.append('#define HUD_PANEL_'+corner.upper()+' '+str(image('menu_'+name,raw=bti(menu_archive['timg/'+name]))))
    if banner:
        from disc_asset import decode_banner
        rgb=decode_banner(banner);rgba=b''.join(rgb[i:i+3]+b'\xff' for i in range(0,len(rgb),3));header.append('#define HUD_TITLE_LOGO '+str(image('opening_logo',raw=(96,32,rgba))))
    if title_archive:
        from title_assets import build_ui
        layouts['title']=build_ui(title_archive,image,title_output)
    header.append('#define HUD_IMAGE_COUNT '+str(len(metadata)))
    if len(metadata)>256:raise ValueError('Too many native HUD images')
    return (struct.pack('<4s3I',b'P2U1',len(payload),zlib.crc32(payload),len(metadata))+payload,
            metadata,'\n'.join(header)+'\n',layouts)
