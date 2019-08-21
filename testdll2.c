/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 * Copyright (c) 2019 Pali Rohár <pali.rohar@gmail.com>
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

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <stdio.h>

#include "dlfcn.h"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT int function2( void )
{
    char *error;
    int (*function2_orig)(void);
    printf( "Hello, world! from wrapper library\n" );
    *(void **) (&function2_orig) = dlsym( RTLD_NEXT, "function2" );
    if (!function2_orig)
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from RTLD_NEXT handle: %s\n",
                error ? error : "" );
        return 1;
    }
    if (function2_orig() != 0)
    {
        printf( "ERROR\tOriginal function from RTLD_NEXT handle did not return correct value\n" );
        return 1;
    }
    return 2;
}
