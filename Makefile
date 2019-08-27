#
# dlfcn-win32 Makefile
#
include config.mak
CFLAGS = -Wall -O3 -fomit-frame-pointer

ifeq ($(BUILD_SHARED),yes)
	TARGETS += libdl.dll
	SHFLAGS += -Wl,--out-implib,libdl.dll.a -DSHARED
	INSTALL += shared-install
	TESTS   += test.exe
endif
ifeq ($(BUILD_STATIC),yes)
	TARGETS += libdl.a
	INSTALL += static-install
	TESTS   += test-static.exe
endif
ifeq ($(BUILD_MSVC),yes)
    TARGETS += libdl.lib
	SHFLAGS += -Wl,--output-def,libdl.def
	INSTALL += lib-install
endif

SOURCES  := dlfcn.c
HEADERS  := dlfcn.h

all: $(TARGETS)

libdl.a: $(SOURCES)
	$(CC) $(CFLAGS) -c $^
	$(AR) cru $@ $(SOURCES:%.c=%.o)
	$(RANLIB) $@

libdl.dll: $(SOURCES)
	$(CC) $(CFLAGS) $(SHFLAGS) -shared -o $@ $^

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

test.exe: test.c $(TARGETS)
	$(CC) $(CFLAGS) -o $@ $< libdl.dll.a

test-static.exe: test.c $(TARGETS)
	$(CC) $(CFLAGS) -o $@ $< libdl.a

testdll.dll: testdll.c
	$(CC) $(CFLAGS) -shared -o $@ $^

testdll2.dll: testdll2.c $(TARGETS)
	$(CC) $(CFLAGS) -shared -o $@ $< -L. -ldl

testdll3.dll: testdll3.c
	$(CC) -shared -o $@ $^

	for test in $(TESTS); do $(WINE) $$test || exit 1; done
test: $(TESTS) testdll.dll testdll2.dll testdll3.dll

clean::
	rm -f \
		dlfcn.o \
		libdl.dll libdl.a libdl.def libdl.dll.a libdl.lib libdl.exp \
		tmptest.c tmptest.dll \
		test.exe test-static.exe testdll.dll testdll2.dll testdll3.dll

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
