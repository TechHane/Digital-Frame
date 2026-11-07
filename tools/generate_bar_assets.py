from pathlib import Path
import json, re

def generate(root):
    assets=json.loads((root/'web/bar-assets.json').read_text(encoding='utf-8'))
    palette=[]
    for color in assets['palette']:
        r,g,b=bytes.fromhex(color[1:])
        palette.append((r>>3)<<11 | (g>>2)<<5 | b>>3)
    header='// Generated from web/bar-assets.json.\n#pragma once\n'
    header+='constexpr uint16_t BAR_PALETTE[]={'+','.join(map(str,palette))+'};\n'
    header+='const uint8_t BAR_ICONS[10][256] PROGMEM={\n'
    header+=',\n'.join('{'+','.join(map(str,icon))+'}' for icon in assets['icons'])+'\n};\n'
    glyphs=assets['smoothGlyphs'];chars='0123456789:.-?'
    header+='constexpr char BAR_CHARS[]="'+chars+'";\n'
    header+='constexpr uint8_t BAR_WIDTHS[]={'+','.join(str(glyphs[c]['width']) for c in chars)+'};\n'
    header+='const uint8_t BAR_GLYPHS[14][216] PROGMEM={\n'+',\n'.join('{'+','.join(map(str,glyphs[c]['pixels']))+'}' for c in chars)+'\n};\n'
    (root/'firmware/FotoCerceve/BarAssets.h').write_text(header,encoding='utf-8')
    page=root/'web/index.html'
    value='/*BAR_ASSETS_START*/const barAssets='+json.dumps(assets,separators=(',',':'))+';/*BAR_ASSETS_END*/'
    text,count=re.subn(r'/\*BAR_ASSETS_START\*/.*?/\*BAR_ASSETS_END\*/',lambda _:value,page.read_text(encoding='utf-8'),flags=re.S)
    assert count==1
    page.write_text(text,encoding='utf-8')

if __name__=='__main__':generate(Path(__file__).resolve().parents[1])
