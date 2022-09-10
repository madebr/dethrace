#!/usr/bin/env python

import re
import string
import sys

r = re.compile("g[A-Z][0-9a-zA-Z_]*")

txt = open(sys.argv[1], "r").read()

def repl(m: re.Match) -> str:
    dflt = m.group(0)
    if txt[m.start() - 1] not in "!*&[() \t":
        return dflt
    if txt[m.start()-3:m.start()] == "HV(":
        return dflt
    if txt[m.end()] not in ".,-[]+); \t":
        return dflt
    return f"HV({dflt})"

new_txt, _ = r.subn(repl, txt)

sys.stdout.write(new_txt)
