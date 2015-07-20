# grip_hook.py
# for use with grip, and this patch https://github.com/joeyespo/grip/issues/120
import os
import os.path
import sys

markdownFileName = sys.argv[1]

# assume that the source file has the same name without the .md extension
sourceFileName = markdownFileName[:-3] # strip .md extension

if os.path.isfile(sourceFileName):
    os.system("emdeer.py "+ sourceFileName)
