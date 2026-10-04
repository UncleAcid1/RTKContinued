import struct,sys
import os
SO=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../Android files/rule-the-kingdom-5-11-multi-android/lib/armeabi-v7a/libkingdom.so")
b=open(SO,'rb').read()
phoff=struct.unpack_from('<I',b,0x1c)[0]; phnum=struct.unpack_from('<H',b,0x2c)[0]
segs=[]
for i in range(phnum):
    t,off,va,pa,fs,ms,fl,al=struct.unpack_from('<8I',b,phoff+32*i)
    if t==1: segs.append((va,off,fs))
def rd(ga,n):
    va=ga-0x10000
    for s,o,f in segs:
        if s<=va<s+f: return b[o+va-s:o+va-s+n]
    return None
def u32(ga): return struct.unpack('<I',rd(ga,4))[0]
def cstr(ga):
    d=rd(ga,200); return d[:d.find(b'\0')].decode('latin1') if d else None
