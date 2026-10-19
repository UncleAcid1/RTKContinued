import sys,re
# tools/test_hook.py on|off tools/test_hook.txt : put the temporary fight / window test loop (RTK_T=...) into src/main.cpp or take it out again. Never commit main.cpp with it in.
p='src/main.cpp'; s=open(p).read(); save=sys.argv[2]
if sys.argv[1]=='off':
    a=s.index('    if (const char* T = getenv("RTK_T")) {   // TEMPORARY TEST HOOK'); b=s.index('    } else\n',a)+len('    } else\n')
    open(save,'w').write(s[a:b]); s=s[:a]+s[b:]; s=s.replace('#include <array>\n','',1).replace('#include <set>\n','',1)
else:
    h=open(save).read(); k='    for (int i = 0; i < opt.frames; ++i) frame();\n'; assert s.count(k)==1
    s=s.replace(k,h+k); s=s.replace('#include <cstdio>','#include <array>\n#include <cstdio>\n#include <set>',1)
open(p,"w").write(s); import os,time; os.utime(p,(time.time()+2,time.time()+2))
