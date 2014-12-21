ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES  = libdl.la
libdl_la_SOURCES = dlfcn.c
# Don't build libdl as libdl-0.dll
libdl_la_LDFLAGS = -avoid-version -no-undefined @GEN_DEF@
include_HEADERS = dlfcn.h

$(lib_LTLIBRARIES): @LIBDL_MSVC_LIB@

libdl.lib:
	$(MSVCLIB) -def:libdl.def -out:libdl.lib

install-exec:: @EXTRA_INSTALL@

install-msvc-lib:
	mkdir -p "$(DESTDIR)$(libdir)"
	@echo " $(INSTALL_DATA) $$files '$(DESTDIR)$(libdir)'"
	$(INSTALL_DATA) libdl.lib "$(DESTDIR)$(libdir)"
