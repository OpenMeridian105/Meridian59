TOPDIR=..\..\..
!include $(TOPDIR)\common.mak

SOURCEDIR = .
OUTDIR = .

.SUFFIXES: .bbg

BGFS = neeleme.bgf neelemeX.bgf iunholyt.bgf xeoneru.bgf xeoneruX.bgf \
       pear.bgf peartree.bgf orange.bgf orangetree.bgf cookie.bgf \
       mmdrat.bgf mfdrat.bgf arrowgold.bgf dragon.bgf dragonX.bgf \
       Msnowman.bgf MsnowmanX.bgf woice.bgf iwallice.bgf gshqm.bgf gshqmov.bgf

!include $(RESOURCEDIR)\copy.mak

{$(SOURCEDIR)}.bbg.bgf:
   $(MAKEBGF) -o $(OUTDIR)\$*.bgf @$*.bbg

clean:

