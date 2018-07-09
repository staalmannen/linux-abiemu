# $Id: Makefile 3 2000-10-27 00:56:29Z hch $

SUBDIRS = elfmark lnxstamp mkmnttab # trace

all: ${SUBDIRS}

elfmark::
	@( echo "$@ ===> elfmark"; cd elfmark; ${MAKE} )

lnxstamp::
	@( echo "$@ ===> lnxstamp"; cd lnxstamp; ${MAKE} )

mkmnttab::
	@true # nothing

#trace::
#	@( echo "$@ ===> trace"; cd trace; ${MAKE} )

install clean::
	@for i in ${SUBDIRS}; do \
	    ( echo "$@ ===> $$i" ; cd $$i ; ${MAKE} $@ ); \
	done
