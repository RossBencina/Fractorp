# `emdeer.py`
#
# > "First keep the peace within yourself,
# >  then you can also bring peace to others."
# >          -- Thomas a Kempis, 1420
#
# Outputs source code as markdown.
# Comments prefixed with `textPrefix` become text in the markdown.
# Source code gets quoted as source code.
# The program may be run and output may be captured.
#
# Links:
#  * Markdown cheatsheet:
#      https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet
#
#  * `grip` local markdown preview
#      https://github.com/joeyespo/grip

import os
import sys
import datetime

if len(sys.argv) == 2:
    sourceFileName = sys.argv[1]
else:
    print "usage: emdeer.py sourceFileName"
    #exit(-1)
    # debugging:
    sourceFileName = "01-boxed-function-pointer.cpp"

textPrefix = "/// "

sourceLines = open(sourceFileName, "rt").read().splitlines()

outputFileName = sourceFileName + ".md"
outputFile = open(outputFileName, "wt")

# compile and run the source file.
# possibly we should store this info in special comments at the top
# of the source file.
os.system("g++ "+sourceFileName);
os.system("a.exe > a.output.txt");

programOutputLines = open("a.output.txt", "rt").read().splitlines()

# break the program output into a series of blocks, each keyed by the
# @OUTPUT directive up to : or EOL
#print programOutputLines
outputBlocks = {}
blockKey = ""
blockLines = []
for s in programOutputLines:
    if s.startswith("@OUTPUT"):
        if blockKey: # flush previous block
            outputBlocks[blockKey] = blockLines
            blockLines = []
        blockKey = s.split(":")[0] # start new block
    blockLines.append(s)
if blockKey:
    outputBlocks[blockKey] = blockLines

#print outputBlocks


# Turn the text "inside out" as follows:
# - Lines beginning with "/// " (a.k.a. textPrefix)
#   become plain text. Only the prefix is stripped, to
#   allow for indenting.
# - Lines not beginning with textPrefix are quoted as
#   source code, with the following exceptions:
#       * blank lines adjoining plain text are merged
#         in to the plain text.
#       * comment-prefixed lines which exactly contain
#         an output block key are replaced with the
#         corresponding output block.
#         
# A "blank line" is a line that is empty after stripping
# its start and end of all whitespace.

def isBlank(s):
   return (len(s.strip()) == 0)

def isText(s):
   return s.startswith(textPrefix)

def isCode(s):
    return not isText(s)

def stripPrefix(x):
    return s[len(textPrefix):]

src = sourceLines # use shorter name to save keystrokes

# sweep through lines, forwards, then backwards, extending
# text areas to adjacent blank lines.

def extendText(i, previousLineWasText):
    s = src[i]
    if isBlank(s) and previousLineWasText:
        src[i] = textPrefix # extend text to blank line
        return True
    else:
        return isText(s)

prevLineWasText = True  
N = len(sourceLines)
for i in range(0,N): # forward
    prevLineWasText = extendText(i, prevLineWasText)

previousLineWasText = True # backward
for i in range(N-1,-1,-1):
    prevLineWasText = extendText(i, prevLineWasText)


codeBegin = "```c++"
codeEnd = "```"

def outputLine(s):
    outputFile.write(s+"\n")
    #print s;

def outputText(s):
    ss = s.strip()
    if ss in outputBlocks:
        b = outputBlocks[ss]
        outputLine("> ```")
        for x in b:
            outputLine("> "+x)
        outputLine("> ```")
    else:
        outputLine(s)

isOutputting = "text"
for s in src:
    if isOutputting == "text":
        if isText(s):
            outputText(stripPrefix(s))
        else:
            outputLine(codeBegin)
            outputLine(s)
            isOutputting = "code"
    else: # "code"
        if isCode(s):
            outputLine(s)
        else:
            outputLine(codeEnd)
            outputText(stripPrefix(s))
            isOutputting = "text"
if isOutputting == "code":
    outputLine(codeEnd)


outputLine("")
outputLine("Generated from [`%s`](%s) by [`emdeer.py`](emdeer.py) at UTC %s"%(sourceFileName,sourceFileName,datetime.datetime.utcnow()))

outputFile.close()


