#
# makefile for blakserv
#

TOPDIR=..
WARNINGS_AS_ERRORS = /WX-
!include $(TOPDIR)\common.mak

!ifdef RELEASE
OUTDIR = x64\release
!else
OUTDIR = x64\debug
!endif

LIB = $(BLAKLIBDIR)\x64;$(BLAKLIBDIR);$(LIB)

# ----------------------------------------------------------------------
# Additional compiler flags (see common.mak)
# /TP           Compile as C++ code
# /arch:SSE2           Use SSE2 instructions (VS2013+ default to SSE2)
# /Zc:strictStrings-   Avoid legacy string-literal const errors in the nmake path
# /wd5055              Ignore new enum/floating-point deprecation warnings
CFLAGS = $(CFLAGS) /arch:SSE2 /Zc:strictStrings- /wd5055 /DBLAK_USE_POSTGRESQL /I$(BLAKINCLUDEDIR)\postgres

# ----------------------------------------------------------------------
# Additional linker flags (see common.mak)
# /SUBSYSTEM:WINDOWS",5.01" UI Windows XP (5.01)
# /STACK  Stacksize in bytes
# /map    Generate mapfile
# /MACHINE:X64  Explicitly target the 64-bit linker output
LINKFLAGS = $(LINKFLAGS) /MACHINE:X64 /SUBSYSTEM:WINDOWS",6.00" /STACK:0x180000 /map /DEBUG

SOURCEDIR = .

LIBS = gdi32.lib user32.lib wsock32.lib winmm.lib comctl32.lib libpq.lib ws2_32.lib jansson.lib

OBJS =  \
    $(OUTDIR)\main.obj \
    $(OUTDIR)\main_windows.obj \
    $(OUTDIR)\loadkod.obj \
    $(OUTDIR)\class.obj \
    $(OUTDIR)\message.obj \
    $(OUTDIR)\object.obj \
    $(OUTDIR)\sendmsg.obj \
    $(OUTDIR)\roofile.obj \
    $(OUTDIR)\bufpool.obj \
    $(OUTDIR)\ccode.obj \
    $(OUTDIR)\channel.obj \
    $(OUTDIR)\list.obj \
    $(OUTDIR)\timer.obj \
    $(OUTDIR)\session.obj \
    $(OUTDIR)\loadrsc.obj \
    $(OUTDIR)\blakres.obj \
    $(OUTDIR)\roomdata.obj \
    $(OUTDIR)\commcli.obj \
    $(OUTDIR)\string.obj \
    $(OUTDIR)\async.obj \
    $(OUTDIR)\async_windows.obj \
    $(OUTDIR)\loadgame.obj \
    $(OUTDIR)\game.obj \
    $(OUTDIR)\term.obj \
    $(OUTDIR)\account.obj \
    $(OUTDIR)\loadacco.obj \
    $(OUTDIR)\saveacco.obj \
    $(OUTDIR)\savestr.obj \
    $(OUTDIR)\loadstr.obj \
    $(OUTDIR)\nameid.obj \
    $(OUTDIR)\time.obj \
    $(OUTDIR)\dllist.obj \
    $(OUTDIR)\trysync.obj \
    $(OUTDIR)\saveall.obj \
    $(OUTDIR)\loadall.obj \
    $(OUTDIR)\synched.obj \
    $(OUTDIR)\motd.obj \
    $(OUTDIR)\admin.obj \
    $(OUTDIR)\garbage.obj \
    $(OUTDIR)\kodbase.obj \
    $(OUTDIR)\savegame.obj \
    $(OUTDIR)\user.obj \
    $(OUTDIR)\system.obj \
    $(OUTDIR)\resync.obj \
    $(OUTDIR)\gamelock.obj \
    $(OUTDIR)\config.obj \
    $(OUTDIR)\apndfile.obj \
    $(OUTDIR)\admincons.obj \
    $(OUTDIR)\builtin.obj \
    $(OUTDIR)\version.obj \
    $(OUTDIR)\systimer.obj \
    $(OUTDIR)\memory.obj \
    $(OUTDIR)\interface_windows.obj \
    $(OUTDIR)\intrlock.obj \
    $(OUTDIR)\chanbuf.obj \
    $(OUTDIR)\debug.obj \
    $(OUTDIR)\saversc.obj \
    $(OUTDIR)\adminfn.obj \
    $(OUTDIR)\table.obj \
    $(OUTDIR)\parsecli.obj \
    $(OUTDIR)\rscload.obj \
    $(OUTDIR)\crc.obj \
    $(OUTDIR)\md5.obj \
    $(OUTDIR)\maintenance.obj \
    $(OUTDIR)\block.obj \
    $(OUTDIR)\stringinthash.obj \
    $(OUTDIR)\intstringhash.obj \
    $(OUTDIR)\files.obj \
    $(OUTDIR)\sprocket.obj \
    $(OUTDIR)\mutex_windows.obj \
    $(OUTDIR)\thdmsgqueue_windows.obj \
    $(OUTDIR)\database_pg.obj \
    $(OUTDIR)\astar.obj \

all : makedirs $(OUTDIR)\blakserv.exe

$(OUTDIR)\rscload.obj : $(TOPDIR)\util\rscload.c
    $(CC) -Fo$(OUTDIR)/ $(CFLAGS) /Fd$(OUTDIR)\ /FpCpch -c $**

$(OUTDIR)\crc.obj : $(TOPDIR)\util\crc.c
    $(CC) -Fo$(OUTDIR)/ $(CFLAGS) /Fd$(OUTDIR)\ /FpCpch -c $**

$(OUTDIR)\md5.obj : $(TOPDIR)\util\md5.c
    $(CC) -Fo$(OUTDIR)/ $(CFLAGS) /Fd$(OUTDIR)\ /Fpmd5pch -c $**

$(OUTDIR)\blakserv.exe: $(OBJS) $(OUTDIR)\blakserv.res
    $(CC) $(CFLAGS) -Fo$(OUTDIR)/ /FpCpch /Fd$(OUTDIR)\ -c $(SOURCEDIR)\version.c
    $(LINK)	$** $(LIBS) -OUT:$@ $(LINKFLAGS)
    $(CP) $@ $(BLAKSERVRUNDIR)
    $(CP) $(OUTDIR)\*.pdb $(BLAKSERVRUNDIR) >nul
    $(CP) $(OUTDIR)\*.map $(BLAKSERVRUNDIR) >nul
    call "$(SOURCEDIR)\copy_postgres_runtime.bat" "$(BLAKBINDIR)" "$(BLAKSERVRUNDIR)" >nul

!include $(TOPDIR)\rules.mak
