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
	LIBCMD=lib
	INSTALL+=lib-install
else
	LIBCMD=echo ignoring lib
endif
ifeq ($(DO_STRIP),yes)
	STRIP=strip
else
	STRIP=echo ignoring strip
endif

all: $(TARGETS)

dlfcn.o:
	$(CC) -o dlfcn.o -c dlfcn.c -O3 -fomit-frame-pointer

libdl.a: dlfcn.o
	ar cru libdl.a dlfcn.o
	ranlib libdl.a

libdl.dll: dlfcn.o
	$(CC) $(SHFLAGS) -shared -o libdl.dll dlfcn.o
	$(LIBCMD) /machine:i386 /def:libdl.def

shared-install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp libdl.dll $(DESTDIR)$(PREFIX)/bin
	$(STRIP) $(DESTDIR)$(PREFIX)/bin/libdl.dll
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp libdl.dll.a $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp dlfcn.h $(DESTDIR)$(PREFIX)/include

static-install:
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp libdl.a $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp dlfcn.h $(DESTDIR)$(PREFIX)/include

lib-install:
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp libdl.lib $(DESTDIR)$(PREFIX)/lib

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
