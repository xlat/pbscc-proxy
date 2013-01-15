!include <win32.mak>

PRJNAME=pbscc

LIB_FILES= KERNEL32.lib USER32.lib SHELL32.LIB ADVAPI32.LIB wininet.lib lib\pbscc_expat.lib


CPP_FILES= pbscc.cpp todo.cpp easystr.cpp entries.cpp filecmp.cpp conproc.cpp login.cpp filecpy.cpp svnstat.cpp test.cpp

OBJS= $(CPP_FILES:.cpp=.obj)

all: ver x_res x_exe test

ver: 
	-SubWCRev . version.hxx tmp\version.h

.cpp.obj:
	$(cc) -Ox $(cflags) $(cvarsdll) -DSTRICT -GX -YX -DNODEBUG $<

x_res: 
	rc $(rcvars) $(rcflags) -r -fo $(PRJNAME).res $(PRJNAME).rc

x_exe: $(OBJS)
	$(link) /dll /RELEASE /VERSION:2.4 -out:$(PRJNAME).dll $(OBJS) $(PRJNAME).res $(LIB_FILES) /DEF:$(PRJNAME).def

test: $(OBJS)
	$(link) /RELEASE -out:$(PRJNAME)_test.exe $(OBJS) $(PRJNAME).res $(LIB_FILES)

clean:
	-del *.obj
	-del *.pch
	-del *.exp
	-del *.lib
	-del *.res
