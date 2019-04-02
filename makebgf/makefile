# makefile for makebgf

TOPDIR=..
!include $(TOPDIR)\common.mak

# ----------------------------------------------------------------------
# Additional compiler flags (see common.mak)
# /TP           Compile as C++ code
# /arch:SSE2    Use SSE2 instructions (VS2013+ default to SSE2)
CFLAGS = $(CFLAGS) /arch:SSE2 /TP

# ----------------------------------------------------------------------
# Additional linker flags (see common.mak)
# /SUBSYSTEM:CONSOLE",5.01" NO-UI Windows XP (5.01)
LINKFLAGS = $(LINKFLAGS) /SUBSYSTEM:CONSOLE",5.01"

OBJS = \
    $(OUTDIR)\makebgf.obj \
    $(OUTDIR)\dibutil.obj \
    $(OUTDIR)\writebgf.obj

LIBS = zlib.lib

all: makedirs $(OUTDIR)\makebgf.exe

$(OUTDIR)\makebgf.exe: $(OBJS)
    $(LINK) $(LINKFLAGS) $** $(LIBS) -out:$@
    $(CP) $@ $(BLAKBINDIR)

!include $(TOPDIR)\rules.mak
