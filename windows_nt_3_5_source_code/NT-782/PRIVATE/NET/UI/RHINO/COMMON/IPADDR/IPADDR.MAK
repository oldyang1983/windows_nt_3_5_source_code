# Microsoft Visual C++ generated build script - Do not modify

PROJ = IPADDR
DEBUG = 1
PROGTYPE = 3
CALLER = 
ARGS = 
DLLS = 
ORIGIN = MSVCNT
ORIGIN_VER = 1.00
PROJPATH = E:\NT\PRIVATE\NET\UI\RHINO\COMMON\IPADDR\ 
USEMFC = 1
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = 
FIRSTCPP = IPADRCLS.CPP
RC = rc
CFLAGS_D_LIB32 = /nologo /W3 /Z7 /YX /D "_DEBUG" /D "IPDLL" /D "WIN32" /D "_X86_" /D "_WINDOWS" /D "_VC100" /I "..\ipadrdll" /FR /MT /Fp"IPADDR.PCH"
CFLAGS_R_LIB32 = /nologo /W3 /YX /O2 /D "NDEBUG" /D "WIN32" /D "_X86_" /D "_WINDOWS" /D "_VC100" /I "..\ipadrdll" /FR /MT /Fp"IPADDR.PCH"
LFLAGS_D_LIB32 = /NOLOGO
LFLAGS_R_LIB32 = /NOLOGO
RCFLAGS32 = 
D_RCDEFINES32 = -d_DEBUG
R_RCDEFINES32 = -dNDEBUG
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_LIB32)
LFLAGS = 
LIBS = 
LFLAGS_LIB=$(LFLAGS_D_LIB32)
MAPFILE_OPTION = 
RCDEFINES = $(D_RCDEFINES32)
!else
CFLAGS = $(CFLAGS_R_LIB32)
LFLAGS = 
LIBS = 
MAPFILE_OPTION = 
LFLAGS_LIB=$(LFLAGS_R_LIB32)
RCDEFINES = $(R_RCDEFINES32)
!endif
SBRS = IPADRCLS.SBR


IPADRCLS_DEP =  \
	e:\nt\private\net\ui\rhino\common\ipaddr\ipaddr.hpp \
	E:\NT\PRIVATE\NET\UI\RHINO\COMMON\ipadrdll\ipadd.h \
	E:\NT\PRIVATE\NET\UI\RHINO\COMMON\ipadrdll\ipaddr.h


all:	$(PROJ).LIB $(PROJ).BSC

IPADRCLS.OBJ:	IPADRCLS.CPP $(IPADRCLS_DEP)
	$(CPP) $(CFLAGS) $(CPPCREATEPCHFLAG) /c IPADRCLS.CPP

$(PROJ).LIB:	IPADRCLS.OBJ $(OBJS_EXT) $(LIBS_EXT)
	echo >NUL @<<$(PROJ).CRF
IPADRCLS.OBJ 


<<
	if exist $@ del $@
	link -LIB @$(PROJ).CRF

$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
