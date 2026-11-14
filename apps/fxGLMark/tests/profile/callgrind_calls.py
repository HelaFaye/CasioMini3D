#!/usr/bin/env python3
# Sum call counts per callee from a callgrind output file, filtered by a regex:
#   callgrind_calls.py <callgrind.out> <regex>
# Useful for counting soft-float calls (e.g. "__[a-z]+sf[0-9]$") per frame.
import sys, re, collections
names={}; calls=collections.Counter(); cfn=None
for line in open(sys.argv[1]):
    m=re.match(r'(c?fn)=\((\d+)\)(?: (.*))?',line)
    if m:
        kind,i,n=m.groups()
        if n: names[i]=n.strip()
        if kind=='cfn': cfn=names.get(i,i)
        continue
    m=re.match(r'calls=(\d+)',line)
    if m and cfn: calls[cfn]+=int(m.group(1))
pat=re.compile(sys.argv[2])
for n,c in sorted(calls.items(), key=lambda kv:-kv[1]):
    if pat.search(n): print(f"{c:>12,}  {n[:110]}")
