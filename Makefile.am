AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I build-aux
SUBDIRS=scripts doc test

if !FAKEROOT_OSTREE
LIBCOMMUNICATE=libcommunicate.la
noinst_LTLIBRARIES = libcommunicate.la libmacosx.la
if FAKEROOT_FAKENET
libcommunicate_la_SOURCES = communicate.c communicate-tcp.c
else !FAKEROOT_FAKENET
libcommunicate_la_SOURCES = communicate.c communicate-sysv.c
endif !FAKEROOT_FAKENET

libmacosx_la_SOURCES = libfakeroot_inode64.c libfakeroot_unix2003.c patchattr.h
endif

lib_LTLIBRARIES=libfakeroot.la
libfakeroot_la_SOURCES=libfakeroot.c statconv/glibc/linux/alpha/stats.h wrapdef.h  wrapstruct.h communicate.h
libfakeroot_la_LDFLAGS=-release 0
if MACOSX
libfakeroot_la_DEPENDENCIES=wrapdef.h wrapstruct.h $(LIBCOMMUNICATE) libmacosx.la $(LTLIBOBJS)
libfakeroot_la_LIBADD = $(LIBCOMMUNICATE) libmacosx.la $(LTLIBOBJS)
else !MACOSX
libfakeroot_la_DEPENDENCIES=wrapdef.h wrapstruct.h $(LIBCOMMUNICATE) $(LTLIBOBJS)
libfakeroot_la_LIBADD = $(LIBCOMMUNICATE) $(LTLIBOBJS)
endif !MACOSX

if !FAKEROOT_OSTREE
bin_PROGRAMS=faked
faked_SOURCES = faked.c
faked_LDADD = libcommunicate.la
endif

simple_SOURCES=simple.c
noinst_PROGRAMS=simple

CLEANFILES= wrapdef.h wrapstruct.h wrapped.h wraptmpf.h
DISTCLEANFILES = fakerootconfig.h

EXTRA_DIST=wrapawk wrapawk_macosx wrapfunc.inp           \
	   message.h \
	   DEBUG BUGS \
	   setenv.c \
	   preroll

CLEAN_FILES=fakerootconfig.h

if MACOSX
wrapped.h wrapdef.h wrapstruct.h wraptmpf.h:wrapawk_macosx wrapfunc.inp
	awk -f $(srcdir)/wrapawk_macosx < $(srcdir)/wrapfunc.inp
else !MACOSX
wrapped.h wrapdef.h wrapstruct.h wraptmpf.h:wrapawk wrapfunc.inp
	awk -f $(srcdir)/wrapawk < $(srcdir)/wrapfunc.inp
endif !MACOSX

libfakeroot.lo:libfakeroot.c wrapdef.h wrapstruct.h wraptmpf.h

fakerootconfig.h: ./config.status
	CONFIG_FILES= CONFIG_HEADERS= /bin/sh ./config.status


#Local variables:
#mode: makefile
#End:
