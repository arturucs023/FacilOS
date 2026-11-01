#!/usr/bin/env python3
"""Teclea en la VM FacilOSDB via keyboardputstring + Enter por scancode.
Uso: vboxkeys.py 'texto con {RET}' shot.png
"""
import subprocess, sys, time
VBM = r"C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
VM = "FacilOS13"
def putstring(s):
    subprocess.run([VBM, "controlvm", VM, "keyboardputstring", s],
                   capture_output=True)
    time.sleep(1.5)
def enter():
    subprocess.run([VBM, "controlvm", VM, "keyboardputscancode", "1c"],
                   capture_output=True)
    time.sleep(0.6)
    subprocess.run([VBM, "controlvm", VM, "keyboardputscancode", "9c"],
                   capture_output=True)
    time.sleep(1.5)
txt = sys.argv[1]
for part in txt.split("{RET}"):
    if part:
        putstring(part)
    enter()
time.sleep(2)
if len(sys.argv) > 2:
    subprocess.run([VBM, "controlvm", VM, "screenshotpng", sys.argv[2]],
                   capture_output=True)
    print("shot:", sys.argv[2])
