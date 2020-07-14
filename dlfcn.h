/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 *
 * dlfcn-win32 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * dlfcn-win32 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlfcn-win32; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
