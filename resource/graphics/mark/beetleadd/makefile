TOPDIR=..\..\..\..
!include $(TOPDIR)\common.mak

SOURCEDIR = .
OUTDIR = .

.SUFFIXES: .bbg

BGFS = gbeetle.bgf gbeetleX.bgf

!include $(RESOURCEDIR)\copy.mak

{$(SOURCEDIR)}.bbg.bgf:
	$(MAKEBGF) -o $(OUTDIR)\$*.bgf @$*.bbg
