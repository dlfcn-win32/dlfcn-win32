#
# dlfcn-win32 Makefile
#
include config.mak
CFLAGS = -Wall -O3 -fomit-frame-pointer -Isrc

ifeq ($(BUILD_SHARED),yes)
	TARGETS += libdl.dll
	SHFLAGS += -Wl,--out-implib,libdl.dll.a
	INSTALL += shared-install
	TESTS   += test.exe test-dladdr.exe
endif
ifeq ($(BUILD_STATIC),yes)
	TARGETS += libdl.a
	INSTALL += static-install
	TESTS   += test-static.exe test-dladdr-static.exe
endif
ifeq ($(BUILD_MSVC),yes)
    TARGETS += libdl.lib
	SHFLAGS += -Wl,--output-def,libdl.def
	INSTALL += lib-install
endif

SOURCES  := src/dlfcn.c
HEADERS  := src/dlfcn.h

all: $(TARGETS)

libdl.a: $(SOURCES)
	$(CC) $(CFLAGS) -o $(^:%.c=%.o) -c $^
	$(AR) cru $@ $(SOURCES:%.c=%.o)
	$(RANLIB) $@

libdl.dll: $(SOURCES)
	$(CC) $(CFLAGS) $(SHFLAGS) -DDLFCN_WIN32_SHARED -shared -o $@ $^

libdl.lib: libdl.dll
	$(LIBCMD) /machine:i386 /def:libdl.def

include-install: $(HEADERS)
	mkdir -p $(DESTDIR)$(incdir)
	install -m 644 $^ "$(DESTDIR)$(incdir)"

shared-install: include-install
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp libdl.dll $(DESTDIR)$(prefix)/bin
	$(STRIP) $(DESTDIR)$(prefix)/bin/libdl.dll
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.dll.a $(DESTDIR)$(libdir)

static-install: include-install
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.a $(DESTDIR)$(libdir)

lib-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libdl.lib $(DESTDIR)$(libdir)

install: $(INSTALL)

test.exe: tests/test.c $(TARGETS)
	$(CC) $(CFLAGS) -o $@ $< libdl.dll.a

test-static.exe: tests/test.c $(TARGETS)
	$(CC) $(CFLAGS) -o $@ $< libdl.a

test-dladdr.exe: tests/test-dladdr.c $(TARGETS)
	$(CC) $(CFLAGS) -Wl,--export-all-symbols -DDLFCN_WIN32_SHARED -o $@ $< libdl.dll.a

test-dladdr-static.exe: tests/test-dladdr.c $(TARGETS)
	$(CC) $(CFLAGS) -Wl,--export-all-symbols -o $@ $< libdl.a

testdll.dll: tests/testdll.c
	$(CC) $(CFLAGS) -shared -o $@ $^

testdll2.dll: tests/testdll2.c $(TARGETS)
	$(CC) $(CFLAGS) -shared -o $@ $< -L. -ldl

testdll3.dll: tests/testdll3.c
	$(CC) -shared -o $@ $^

test: $(TARGETS) $(TESTS) testdll.dll testdll2.dll testdll3.dll
	for test in $(TESTS); do $(WINE) $$test || exit 1; done

clean::
	rm -f \
		src/dlfcn.o \
		libdl.dll libdl.a libdl.def libdl.dll.a libdl.lib libdl.exp \
		tmptest.c tmptest.dll \
		test-dladdr.exe test-dladdr-static.exe \
		test.exe test-static.exe testdll.dll testdll2.dll testdll3.dll

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
