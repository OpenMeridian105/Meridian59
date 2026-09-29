# makefile for jansson

TOPDIR=..
!include $(TOPDIR)\common.mak

!ifndef ARCH
!if "$(VSCMD_ARG_TGT_ARCH)" == "x64"
ARCH = x64
!else
ARCH = x86
!endif
!endif

!if "$(ARCH)" == "x64"
OUTDIR = x64\$(OUTDIR)
JANSSONLIBDIR = $(BLAKLIBDIR)\x64
CFLAGS = $(CFLAGS) /wd4267 /wd4334
!else
JANSSONLIBDIR = $(BLAKLIBDIR)
!endif

# ----------------------------------------------------------------------
# Additional compiler flags (see common.mak)
# /TC           Compile as C code
# /arch:SSE2    Use SSE2 instructions (VS2013+ default to SSE2)
# /wd4005       Ignore macro redefinition warning
CFLAGS = $(CFLAGS) /arch:SSE2 /TC /D_USING_V110_SDK71_ /wd4005

# ----------------------------------------------------------------------
# Additional static linker flags (see common.mak)
# /SUBSYSTEM:CONSOLE",5.01" NO-UI Windows XP (5.01)
LINKSTFLAGS = $(LINKSTFLAGS) /SUBSYSTEM:CONSOLE",5.01"

OBJS = \
    $(OUTDIR)\dump.obj \
    $(OUTDIR)\error.obj \
    $(OUTDIR)\hashtable.obj \
    $(OUTDIR)\hashtable_seed.obj \
    $(OUTDIR)\load.obj \
    $(OUTDIR)\memory.obj \
    $(OUTDIR)\pack_unpack.obj \
    $(OUTDIR)\strbuffer.obj \
    $(OUTDIR)\strconv.obj \
    $(OUTDIR)\utf.obj \
    $(OUTDIR)\value.obj

all: makedirs makelibdirs $(JANSSONLIBDIR)\jansson.lib

makelibdirs:
	-@mkdir $(JANSSONLIBDIR) >nul 2>&1

$(JANSSONLIBDIR)\jansson.lib: $(OBJS)
    $(LINKST) $(LINKSTFLAGS) $** $(LIBS) -out:$@
		
!include $(TOPDIR)\rules.mak
