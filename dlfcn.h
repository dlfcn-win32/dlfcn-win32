/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef DLFCN_H
#define DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DLFCN_WIN32_EXPORTS)
#   define DLFCN_EXPORT __declspec(dllexport)
#else
#   define DLFCN_EXPORT
#endif

#include <wchar.h>

/* Relocations are performed when the object is loaded. */
#define RTLD_NOW    0

/* Relocations are performed at an implementation-defined time.
 * Windows API does not support lazy symbol resolving (when first reference
 * to a given symbol occurs). So RTLD_LAZY implementation is same as RTLD_NOW.
 */
#define RTLD_LAZY   RTLD_NOW

/* All symbols are available for relocation processing of other modules. */
#define RTLD_GLOBAL (1 << 1)

/* All symbols are not made available for relocation processing by other modules. */
#define RTLD_LOCAL  (1 << 2)

/* These two were added in The Open Group Base Specifications Issue 6.
 * Note: All other RTLD_* flags in any dlfcn.h are not standard compliant.
 */

/* The symbol lookup happens in the normal global scope. */
#define RTLD_DEFAULT    ((void *)0)

/* Specifies the next object after this one that defines name. */
#define RTLD_NEXT       ((void *)-1)

/* Open a symbol table handle (file path is in wide character). */
DLFCN_EXPORT void *wdlopen(const wchar_t *file, int mode);

/* Open a symbol table handle (ASCII file path only). */
DLFCN_EXPORT void *dlopen(const char *file, int mode);

/* Close a symbol table handle. */
DLFCN_EXPORT int dlclose(void *handle);

/* Get the address of a symbol from a symbol table handle. */
DLFCN_EXPORT void *dlsym(void *handle, const char *name);

/* Get diagnostic information. */
DLFCN_EXPORT char *dlerror(void);

#ifdef __cplusplus
}
#endif

#endif /* DLFCN_H */
