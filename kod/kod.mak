#
# Makefile piece included by each kod directory makefile. The top level
# makefile reproduces this with some commands to run after recursing.
#

.SUFFIXES : .kod .lkod

BCFLAGS = -d -I $(KODINCLUDEDIR) -K $(KODDIR)\kodbase.txt

.lkod.kod::
.kod.bof::
	@$(BC) $(BCFLAGS) $<
	@for %i in ($<) do @$(KODDIR)\bin\instbofrsc $(TOPDIR) $(BLAKSERVRUNDIR) %i

all : $(BOFS)
# Make sure all listed bofs exist - catch typos.
	@for %i in ($(BOFS:.bof=.kod)) do @if NOT EXIST %i (echo error: Missing kod file %i with makefile entry!)

	@for %i in ($(BOFS:.bof=)) do @if EXIST %i (echo Building %i & cd %i & $(MAKE) /$(MAKEFLAGS) TOPDIR=..\$(TOPDIR) & cd ..)


$(BOFS): $(DEPEND)

clean :
	@-$(RM) *.bof *.rsc >nul 2>&1
	@-for %i in ($(BOFS:.bof=.)) do @if EXIST %i (cd %i & $(MAKE) /$(MAKEFLAGS) TOPDIR=..\$(TOPDIR) clean & cd .. )
