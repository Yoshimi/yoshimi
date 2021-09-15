#!/usr/bin/env python

def scanfile(f):
    ID = f.readline()
    f.close()
    ID = ID.strip()
    pos = ID.find(' ')
    if (pos > 0):
        ID = ID[:pos]
    doc = open("indexref.html","r")
    newdoc = open("doc/yoshimi_user_guide/index.html","w")
    line = "start"
    count = 0;
    tofind = '<h1 style="text-align: center;">The Yoshimi User Guide V'
    while line != "":
        count += 1;
        line = doc.readline()
        if line:
            if line.find(tofind) > 0:
                newdoc.write('    '+tofind+ID+'</h1>\n')
            else:
                newdoc.write(line)


f = open("src/version.txt","r+")
if f:
    scanfile(f)
