#!/usr/bin/python3

# this is a simple script

import os
import sys
import shutil
import configparser

PREVIEW = False
REDO = False
datadir = sys.argv[1]
graphicsdir = os.path.join(datadir, 'graphics')
outdir = sys.argv[2]

for dirpath, _, files in os.walk(datadir, topdown=True):
    outpath = outdir+dirpath[len(datadir):]
    os.makedirs(outpath, exist_ok=True)

    if dirpath.endswith('fonts/'):
        print('found fonts dir', dirpath)
        is_fonts_dir = True
        texture_1x = set()

        for fn in files:
            if fn.endswith('.ini'):
                rfn = os.path.join(dirpath, fn)

                font = configparser.ConfigParser(inline_comment_prefixes=';')
                font.read(rfn)

                if 'font-map' in font:
                    if 'texture-scale' in font['font-map'] and font['font-map']['texture-scale'].strip() != '1':
                        if 'texture' in font['font-map']:
                            tex_string = font['font-map']['texture'].strip().strip('\"')
                            print(f"{tex_string} is 1x")
                            texture_1x.add(tex_string)

    else:
        is_fonts_dir = False

    for fn in files:
        rfn = os.path.join(dirpath, fn)
        if not os.path.isfile(rfn): continue
        destfn = os.path.join(outpath, fn)
        bmpfn = os.path.join(outpath, fn[:-3]+'bmp')
        t3xfn = os.path.join(outpath, fn[:-3]+'t3x')

        is_1x = is_fonts_dir and fn in texture_1x

        if is_1x:
            downscale = "-sample 100%"
        else:
            downscale = "-sample 50%"

        if not REDO and not is_fonts_dir and (os.path.isfile(destfn) or os.path.isfile(destfn+'.wav') or ((fn.endswith('.gif') or fn.endswith('.png')) and os.path.isfile(t3xfn))): continue

        print(rfn)
        if fn.endswith('.png'):
            os.system(f'convert {downscale} "{rfn}" "{bmpfn}"')
        elif fn.endswith('m.gif') and os.path.isfile(rfn[:-5]+'.gif'):
            continue
        elif fn.endswith('.gif'):
            maskfn = rfn[:-4]+'m.gif'
            ftype = fn[:fn.rfind('-')]
            altmaskfn_gif = os.path.join(graphicsdir, ftype, fn[:-4]+'m.gif')
            altmaskfn_png = os.path.join(graphicsdir, ftype, fn[:-4]+'.png')
            if os.path.isfile(maskfn):
                os.system(f'convert "{rfn}" "{maskfn}" -alpha Off -compose CopyOpacity -composite -channel a -negate +channel {downscale} "{bmpfn}"')
            elif os.path.isfile(altmaskfn_gif):
                if os.popen(f'identify -format "%[fx:w*2],%[fx:h*2]" "{rfn}"').read() == os.popen(f'identify -format "%[fx:w*2],%[fx:h*2]" "{altmaskfn_gif}"').read():
                    os.system(f'convert "{rfn}" "{altmaskfn_gif}" -alpha Off -compose CopyOpacity -composite -channel a -negate +channel {downscale} "{bmpfn}"')
                else:
                    os.system(f'convert {downscale} "{rfn}" "{bmpfn}"')
            elif os.path.isfile(altmaskfn_png):
                if os.popen(f'identify -format "%[fx:w*2],%[fx:h*2]" "{rfn}"').read() == os.popen(f'identify -format "%[fx:w*2],%[fx:h*2]" "{altmaskfn_png}"').read():
                    os.system(f'convert "{rfn}" "{altmaskfn_png}" -alpha On -compose CopyOpacity -composite {downscale} "{bmpfn}"')
                else:
                    os.system(f'convert {downscale} "{rfn}" "{bmpfn}"')
            else:
                os.system(f'convert {downscale} "{rfn}" "{bmpfn}"')
        elif fn.endswith('.db'):
            continue
        elif fn.endswith('.ogg') and '/sound/' in rfn:
            os.system(f'ffmpeg -i "{rfn}" "{destfn}.wav"')
            continue
        elif is_fonts_dir and fn.endswith('.ini'):
            shutil.copy(rfn, destfn)
            os.system(f'sed \'s/\\.png/\\.t3x/\' -i "{destfn}"')
            continue
        elif fn == 'sounds.ini':
            shutil.copy(rfn, destfn)
            os.system(f'sed \'s/\\.ogg"/\\.ogg.wav"/\' -i "{destfn}"')
            continue
        elif fn.endswith('.mp3'):
            os.system(f'ffmpeg -i "{rfn}" -aq 1 "{destfn}.ogg"')
            shutil.move(destfn+'.ogg', destfn)
            continue
        else:
            shutil.copy(rfn, destfn)
            continue
        w, h = os.popen(f'identify -format "%[fx:w*2],%[fx:h*2]" "{bmpfn}"').read().split(',')
        open(t3xfn+'.size','w').write(f'{w:>4}\n{h:>4}\n')
        t3xfns = [t3xfn]
        bmpfns = [bmpfn]
        if int(h) > 2048:
            os.system(f'convert "{bmpfn}" -crop {w}x1024 "{bmpfn}%d.bmp"')
            os.remove(bmpfn)
            shutil.move(bmpfn+'0.bmp', bmpfn) #???
            t3xfns.append(t3xfn+'1')
            bmpfns.append(bmpfn+'1.bmp')
            if int(h) > 4096:
                t3xfns.append(t3xfn+'2')
                bmpfns.append(bmpfn+'2.bmp')
        for t3xfn_i, bmpfn_i in zip(t3xfns, bmpfns):
            if PREVIEW:
                pvwfn_i = t3xfn_i+'.bmp'
                os.system(f'tex3ds "{bmpfn_i}" -f rgba8888 -o "{t3xfn_i}" -p "{pvwfn_i}"')
            else:
                if os.system(f'tex3ds "{bmpfn_i}" -f rgba8888 -o "{t3xfn_i}"'):
                    print(f"It didn't work and {t3xfn_i} is missing. (Size: {w}x{h})")
                os.remove(bmpfn_i)

# construct graphics load lists
for dirpath, dirs, files in os.walk(outdir, topdown=True):
    print(dirpath)

    if dirpath.endswith('graphics'):
        l = open(os.path.join(dirpath, 'graphics.list'), 'w')

        for d in dirs:
            if d == 'touchscreen' or d == 'ui':
                continue

            for f in os.listdir(os.path.join(dirpath, d)):
                if not f.endswith('.size'):
                    continue

                abs_f = os.path.join(dirpath, d, f)

                basename = f[:f.find('.')]
                if len(basename.split('-')) != 2:
                    continue

                basename = basename.replace('-', ' ')

                if not basename[:basename.find(' ')] in ('background', 'background2', 'block', 'effect', 'level',
                        'link', 'luigi', 'mario', 'npc', 'path',
                        'peach', 'player', 'scene', 'tile', 'toad',
                        'yoshib', 'yoshit'):
                    continue

                fullname = os.path.join(d, f[:-5])
                l.write(basename+'\n')
                l.write(fullname+'\n')
                l.write(open(abs_f, 'r').read())
                l.write('\n')

        l.close()
        continue
    elif 'graphics' in os.path.split(dirpath):
        continue

    opened = False

    for f in files:
        if not f.endswith('.size'):
            continue

        if not opened:
            l = open(os.path.join(dirpath, 'graphics.list'), 'w')
            opened = True

        abs_f = os.path.join(dirpath, f)

        basename = f[:f.find('.')]
        if len(basename.split('-')) != 2:
            continue

        basename = basename.replace('-', ' ')

        if not basename[:basename.find(' ')] in ('background', 'background2', 'block', 'effect', 'level',
                'link', 'luigi', 'mario', 'npc', 'path',
                'peach', 'player', 'scene', 'tile', 'toad',
                'yoshib', 'yoshit'):
            continue

        fullname = f[:-5]

        l.write(basename+'\n')
        l.write(fullname+'\n')
        l.write(open(abs_f, 'r').read())
        l.write('\n')


    if opened:
        l.close()
