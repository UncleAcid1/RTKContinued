# oda.py lo hi (Ghidra addresses, hex): objdump that range of libkingdom.so, annotate PIC values.
import sys, re, subprocess, glob
sys.path.insert(0, 'tools'); import elfread as e, picsym
SO = glob.glob('../Android files/rule-the-kingdom-5-11-multi-android/lib/armeabi-v7a/libkingdom.so')[0]
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
out = subprocess.run(['objdump', '-d', '--triple=armv7', '--no-show-raw-insn', '--start-address=%#x' % (lo - 0x10000),
                      '--stop-address=%#x' % (hi - 0x10000), SO], capture_output=True, text=True).stdout
GOT = 0x60c1f0
lit = {}
def desc(a):
    n = picsym.name(a)
    s = ''
    try:
        c = e.cstr(a)
        if c and len(c) > 1 and all(32 <= ord(ch) < 127 for ch in c[:40]): s = ' "%s"' % c[:60]
    except Exception: pass
    return ('%x' % a) + (' <%s>' % n if n else '') + s
for l in out.split('\n'):
    m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', l)
    if not m: continue
    fa = int(m.group(1), 16); ga = fa + 0x10000; op = m.group(2); args = m.group(3)
    args = re.sub(r'\s*@.*', '', args)
    note = ''
    mm = re.match(r'(r\d+|lr|r12|ip), \[pc, #(-?0x[0-9a-f]+|-?\d+)\]', args)
    if op.startswith('ldr') and mm:
        v = e.u32(ga + 8 + int(mm.group(2), 0)); lit[mm.group(1)] = v; note = '=%#x' % v
    mm = re.match(r'(r\d+|ip|lr), pc, (r\d+|ip|lr)$', args)
    if op == 'add' and mm and mm.group(2) in lit:
        a = (lit[mm.group(2)] + ga + 8) & 0xffffffff; lit[mm.group(1)] = ('addr', a); note = desc(a)
    mm = re.match(r'(r\d+|ip|lr), \[pc, (r\d+|ip|lr)\]', args)
    if op.startswith('ldr') and mm and isinstance(lit.get(mm.group(2)), int):
        a = (lit[mm.group(2)] + ga + 8) & 0xffffffff
        try: v = e.u32(a); note = '[%s] = %#x' % (desc(a), v) + (' <%s>' % picsym.name(v + 0x10000) if a >= GOT and a < GOT + 0x4000 and picsym.name(v + 0x10000) else '')
        except Exception: note = '[%x]' % a
    mm = re.match(r'(r\d+|ip|lr), \[(r\d+|ip|lr), (r\d+|ip|lr)\]', args)
    if op == 'ldr' and mm:
        b, o = lit.get(mm.group(2)), lit.get(mm.group(3))
        if isinstance(b, tuple) and isinstance(o, int): a = b[1] + o
        elif isinstance(o, tuple) and isinstance(b, int): a = o[1] + b
        else: a = None
        if a is not None:
            try:
                v = e.u32(a); n = picsym.name(v + 0x10000) or picsym.name(v)
                note = 'GOT[%x] = %#x%s' % (a, v, ' <%s>' % n if n else '')
            except Exception: pass
    if op in ('bl', 'blx', 'b') and re.match(r'0x[0-9a-f]+$|[0-9a-f]+$', args.split()[0] if args else ''):
        t = int(args.split()[0], 16) + 0x10000; n = picsym.name(t); note = n or ''
        args = '%x' % t
    mm = re.search(r'\[(r\d+|ip|lr), #(-?0x[0-9a-f]+|-?\d+)\]', args)
    if mm and isinstance(lit.get(mm.group(1)), tuple):
        a = lit[mm.group(1)][1] + int(mm.group(2), 0); note = (note + ' ' if note else '') + '{%s}' % desc(a)
    # a register overwritten loses its literal
    dst = args.split(',')[0].strip()
    if note == '' and dst in lit and op not in ('str', 'strb', 'strh', 'cmp', 'cmn', 'tst', 'vstr', 'push', 'pop', 'b', 'bl'): lit.pop(dst, None)
    print('%08x  %-6s %-40s %s' % (ga, op, args, ('; ' + note) if note else ''))
