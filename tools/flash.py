"""Explicit fresh install or compatible update; never auto-select a port."""
import argparse, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description='ESP32-WROOM 4 MB foto cercevesi yukleyici')
p.add_argument('--port',required=True,help='Target serial port, e.g. COM4 or /dev/ttyUSB0')
p.add_argument('--mode',required=True,choices=['install','update'])
p.add_argument('--baud',type=int,default=115200)
p.add_argument('--dry-run',action='store_true')
a=p.parse_args()
name='FotoCerceve.ino.merged.bin' if a.mode=='install' else 'FotoCerceve.ino.bin'
image=root/'bin'/name
if not image.is_file(): p.error('Firmware image missing: '+name)
cmd=[sys.executable,'-m','esptool','--chip','esp32','--port',a.port,'--baud',str(a.baud),'write-flash']
if a.mode=='install': cmd+=['--erase-all']
cmd+=['0x0' if a.mode=='install' else '0x10000',str(image)]
if a.dry_run:
    print('Mode:',a.mode,'Address:',cmd[-2],'Image:',image.name,'Erase all:',a.mode=='install')
    sys.exit(0)
if a.mode=='install':
    print('ILK KURULUM: Secilen ESP32 icindeki TUM yazilim, ayarlar ve dahili fotograflar silinir.')
    print('SD kart bu islemle silinmez. Port:',a.port)
    if input('Devam etmek icin SIL yazin: ').strip()!='SIL': sys.exit('Iptal edildi.')
subprocess.run(cmd,check=True)
