# Makefile 

TOPDIR=..\..\..
!include $(TOPDIR)\common.mak

SOURCEDIR = .

all: 
	@-for %i in ($(DIRS)) do \
		@if EXIST %i (	cd %i & \
	 	$(MAKE) /$(MAKEFLAGS) & \
		cd .. )
