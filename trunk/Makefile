#
# dlfcn-win32 Makefile
#
include config.mak

ifeq ($(BUILD_SHARED),yes)
	TARGETS+=libdl.dll
	SHFLAGS+=-Wl,--out-implib,libdl.dll.a
	INSTALL+=shared-install
endif
ifeq ($(BUILD_STATIC),yes)
	TARGETS+=libdl.a
	INSTALL+=static-install
endif
ifeq ($(BUILD_MSVC),yes)
	SHFLAGS+=-Wl,--output-def,libdl.def
	INSTALL+=lib-install
endif

all: $(TARGETS)

dlfcn.o:
	$(CC) -o dlfcn.o -c dlfcn.c -Wall -O3 -fomit-frame-pointer

libdl.a: dlfcn.o
	$(AR) cru libdl.a dlfcn.o
	$(RANLIB) libdl.a

libdl.dll: dlfcn.o
	$(CC) $(SHFLAGS) -shared -o libdl.dll dlfcn.o
	$(LIBCMD) /machine:i386 /def:libdl.def

shared-install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp libdl.dll $(DESTDIR)$(prefix)/bin
	$(STRIP) $(DESTDIR)$(prefix)/bin/libdl.dll
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.dll.a $(DESTDIR)$(libdir)
	mkdir -p $(DESTDIR)$(incdir)
	cp dlfcn.h $(DESTDIR)$(incdir)

static-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.a $(DESTDIR)$(libdir)
	mkdir -p $(DESTDIR)$(incdir)
	cp dlfcn.h $(DESTDIR)$(incdir)

lib-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.lib $(DESTDIR)$(libdir)

install: $(INSTALL)

test.exe:
	$(CC) -o test.exe test.c -L. -ldl

testdll.dll:
	$(CC) -shared -o testdll.dll testdll.c

test: $(TARGETS) test.exe testdll.dll
	test.exe

clean::
	rm -f dlfcn.o libdl.dll libdl.a libdl.def libdl.dll.a libdl.lib libdl.exp test.exe testdll.dll

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
