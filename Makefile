all:
	@echo "make"
	@echo "  print usage(this)"
	@echo "make mingwlua"
	@echo "  build nyaos.exe with MinGW and Lua"
	@echo "make mingw"
	@echo "  build nyacus.exe with MinGW"
	@echo "make emxos2"
	@echo "  build nyaos2.exe with emx/gcc on OS/2 Warp"
	@echo "make digitalmars"
	@echo "  build nyados.exe with Digitalmars C++ on Windows"
	@echo "make clean"
	@echo "  clean *.obj *.o *.exe. Please ignore error message"
	@echo "make cleanobj"
	@echo "  clean *.obj *.o. Please ignore error message"
	@echo ""
	@echo "make documents"
	@echo "make release"
	@echo "make nightly"

.SUFFIXES : .cpp .obj .exe .h .res .rc .cpp .h .o
.cpp.obj :
	$(CC) $(CFLAGS) -c $<
.cpp.o :
	$(CC) $(CFLAGS) -c $<

CCC=-DNDEBUG

mingwlua : 
	$(MAKE) CC=gcc CFLAGS="-Wall -O3 $(CCC) -I/usr/local/include -mno-cygwin -D_MSC_VER=1000 -DLUA_ENABLE" O=o \
		LDFLAGS="-s -lole32 -luuid -llua -lstdc++ -L/usr/lib/mingw/ -L../lua-5.1.4" \
		NYAOS.EXE

mingw :
	$(MAKE) CC=gcc CFLAGS="-Wall -O3 $(CCC) -mno-cygwin -D_MSC_VER=1000" O=o \
		       LDFLAGS="-s -lole32 -luuid -lstdc++ -L/usr/lib/mingw/" \
		nyacus.exe


digitalmars :
	make CC=sc NAME=NYADOS CFLAGS="-P -ml -o $(CCC)" O=obj nyados.exe
	# -P ... Pascal linkcage

emxos2 :
	$(MAKE) CC=gcc NAME=NYAOS2 CFLAGS="-O2 -Zomf -Zsys -DOS2EMX $(CCC)" O=obj \
		LDFLAGS=-lstdcpp nyaos2.exe

LUAPATH=../lua-5.1.4
lua :
	$(MAKE) -C $(LUAPATH) CC="gcc -mno-cygwin" generic

clean-lua :
	$(MAKE) -C $(LUAPATH) clean

OBJS=nyados.$(O) nnstring.$(O) nndir.$(O) twinbuf.$(O) mysystem.$(O) keyfunc.$(O) \
	getline.$(O) getline2.$(O) keybound.$(O) dosshell.$(O) nnhash.$(O) \
	writer.$(O) history.$(O) ishell.$(O) scrshell.$(O) wildcard.$(O) cmdchdir.$(O) \
	shell.$(O) shell4.$(O) foreach.$(O) which.$(O) reader.$(O) nnvector.$(O) \
	ntcons.$(O) shellstr.$(O) cmds1.$(O) cmds2.$(O) xscript.$(O) shortcut.$(O) \
	strfork.$(O) lsf.$(O) open.$(O) nua.$(O)

nyaos2.exe : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

NYAOS.EXE : $(OBJS) nyacusrc.$(O)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	objdump -x $< | grep "DLL Name"
	upx -9 $@

NUA.EXE : $(OBJS) nyacusrc.$(O)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	objdump -x $@ | grep "DLL Name"
	upx -9 $@

nyados.exe : $(OBJS)
	$(CC) $(CFLAGS) -o$@ $(OBJS) $(LDFLAGS)

# ���C�����[�`��
nyados.$(O)   : nyados.cpp   nnstring.h getline.h

# ��s����
twinbuf.$(O)  : twinbuf.cpp  getline.h
getline.$(O)  : getline.cpp  getline.h nnstring.h
getline2.$(O) : getline2.cpp getline.h
keybound.$(O) : keybound.cpp getline.h
keyfunc.$(O) : keyfunc.cpp getline.h
dosshell.$(O) : dosshell.cpp getline.h
xscript.$(O) : xscript.cpp

# �C���^�v���^����
shell.$(O)    : shell.cpp    shell.h 
shell4.$(O)   : shell4.cpp   shell.h nnstring.h
scrshell.$(O) : scrshell.cpp shell.h
ishell.$(O)   : ishell.cpp   shell.h ishell.h 
mysystem.$(O) : mysystem.cpp nnstring.h
shellstr.$(O) : shellstr.cpp

# �ʃR�}���h����
cmds1.$(O) : cmds1.cpp shell.h nnstring.h
cmds2.$(O) : cmds2.cpp shell.h nnstring.h
cmdchdir.$(O) : cmdchdir.cpp shell.h nnstring.h nndir.h
foreach.$(O) : foreach.cpp shell.h
lsf.$(O) : lsf.cpp

# ���ʃ��C�u����
nnstring.$(O)  : nnstring.cpp  nnstring.h  nnobject.h
nnvector.$(O)  : nnvector.cpp  nnvector.h  nnobject.h
nnhash.$(O) : nnhash.cpp nnhash.h nnobject.h
strfork.$(O) : strfork.cpp

# ���ˑ����C�u����
writer.$(O) : writer.cpp    writer.h
reader.$(O) : reader.cpp    reader.h
nndir.$(O) : nndir.cpp nndir.h
wildcard.$(O) : wildcard.cpp  nnstring.h nnvector.h nndir.h
ntcons.$(O) : ntcons.cpp
open.$(O) : open.cpp

nua.$(O) : nua.cpp

# ���\�[�X
nyacusrc.$(O)  : nyacus.rc redcat.ico
	windres --output-format=coff -o $@ $<

release :
	$(MAKE) _package VER=`gawk '/^#define VER/{ print $$3 }' nyados.cpp`

nightly :
	$(MAKE) _package VER=`date "+%Y%m%d"`

_package :
	zip nyaos2-$(VER).zip nyaos2.exe nyaos2.txt _nya
	zip nyacus-$(VER).zip nyacus.exe nyacus.txt _nya tagjump.vbs
	zip nyados-$(VER).zip nyados.exe nyados.txt _nya greencat.ico 
	zip nya-$(VER).zip  Makefile *.h *.cpp *.ico *.m4 _nya

documents : nyados.txt nyaos2.txt nyacus.txt

nyados.txt : nya.m4
	m4 -DSHELL=NYADOS $< > $@
nyaos2.txt : nya.m4
	m4 -DSHELL=NYAOS2 $< > $@
nyacus.txt : nya.m4
	m4 -DSHELL=NYACUS $< > $@
clean : 
	del *.obj *.o *.exe || rm *.obj *.o *.exe *.EXE

cleanobj :
	del *.obj *.o || rm *.obj *.o

# vim:set noet ts=8 sw=8 nobk:
