::%1 is for base, %2 is for mine, %3 is for mine title
start /WAIT "c:\Program Files\TortoiseSVN\bin\TortoiseMerge.exe" TortoiseMerge /base:%1 /mine:%2 /minename:%3
del /Q /F %2
