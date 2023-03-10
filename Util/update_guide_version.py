#!/usr/bin/env python

def scanfile(f):
    ID = f.readline()
    f.close()
    ID = ID.strip()
    pos = ID.find(' ')
    if (pos > 0):
        ID = ID[:pos]
    doc = open("../doc/yoshimi_user_guide/index.html","r+")
    line = "start"
    tofind = '<h1 style="text-align: center;">The Yoshimi User Guide V'
    while line != "":
        linepos=doc.tell()
        line = doc.readline()
        if line:
            if line.find(tofind) > 0:
                doc.seek(linepos)
                doc.write('    '+tofind+ID+'<!--'+'x'*(10-len(ID))+'--></h1>\n')
                line = ""


f = open("../src/version.txt","r+")
if f:
    scanfile(f)
