"""Generate embedded assets and build ESP32 firmware. Requires arduino-cli."""
from pathlib import Path
import argparse, os, shutil, subprocess, sys
from generate_bar_assets import generate
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--cli',default=os.environ.get('ARDUINO_CLI','arduino-cli'))
parser.add_argument('--config')
parser.add_argument('--libraries')
parser.add_argument('--build-dir',default=str(ROOT/'.build'))
args=parser.parse_args()
generate(ROOT)
page=(ROOT/'web/index.html').read_text('utf-8')
assert ')FRAMEHTML"' not in page
(ROOT/'firmware/FotoCerceve/WebPage.h').write_text('#pragma once\n#include <pgmspace.h>\nconst char WEB_PAGE[] PROGMEM = R"FRAMEHTML('+page+')FRAMEHTML";\n','utf-8')
# Avoid embedding the builder's personal paths in assertions and log strings.
flags=[]
for origin,target in [(Path.home(),'/builder'),(ROOT,'/project')]:
    for form in {str(origin),origin.as_posix()}:
        flags.append('"-ffile-prefix-map='+form+'='+target+'"')
cmd=[args.cli]
if args.config: cmd+=['--config-file',args.config]
cmd+=['compile','--fqbn','esp32:esp32:esp32:PartitionScheme=no_ota,FlashMode=dio,FlashFreq=40,FlashSize=4M',
      '--build-path',args.build_dir,'--output-dir',str(ROOT/'bin')]
if args.libraries: cmd+=['--libraries',args.libraries]
for language in ['c','cpp','S']:
    cmd+=['--build-property','compiler.'+language+'.extra_flags='+' '.join(flags)]
cmd+=[str(ROOT/'firmware/FotoCerceve')]
subprocess.run(cmd,check=True)
# Release images only: ELF/map files and local build paths stay in .build.
for p in (ROOT/'bin').iterdir():
    if p.suffix in ['.elf','.map']:
        shutil.move(str(p),str(Path(args.build_dir)/p.name))
print('Build complete. See bin/ and docs/ILK-KURULUM.md.')
