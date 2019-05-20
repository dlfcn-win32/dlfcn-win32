#
# dlfcn-win32 Makefile
#
include config.mak
CFLAGS = -Wall -O3 -fomit-frame-pointer

ifeq ($(BUILD_SHARED),yes)
	TARGETS += libdl.dll
	SHFLAGS += -Wl,--out-implib,libdl.dll.a
	CFLAGS  += -DSHARED
	INSTALL += shared-install
endif
ifeq ($(BUILD_STATIC),yes)
	TARGETS += libdl.a
	INSTALL += static-install
endif
ifeq ($(BUILD_MSVC),yes)
    TARGETS += libdl.lib
	SHFLAGS += -Wl,--output-def,libdl.def
	INSTALL += lib-install
endif

LIB_OBJS := dlfcn.o
HEADERS  := dlfcn.h

all: $(TARGETS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

libdl.a: $(LIB_OBJS)
	$(AR) cru $@ $^
	$(RANLIB) libdl.a

libdl.dll: $(LIB_OBJS)
	$(CC) $(SHFLAGS) -shared -o $@ $^

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

test.exe: test.o $(TARGETS)
	$(CC) -o $@ $< -L. -ldl

testdll.dll: testdll.c
	$(CC) -shared -o $@ $^

testdll2.dll: testdll2.c $(TARGETS)
	$(CC) -shared -o $@ $< -L. -ldl

testdll3.dll: testdll3.c
	$(CC) -shared -o $@ $^

test: $(TARGETS) test.exe testdll.dll testdll2.dll testdll3.dll
	$(WINE) test.exe

clean::
	rm -f \
		dlfcn.o \
		libdl.dll libdl.a libdl.def libdl.dll.a libdl.lib libdl.exp \
		tmptest.c tmptest.dll \
		test.exe testdll.dll testdll2.dll testdll3.dll

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
