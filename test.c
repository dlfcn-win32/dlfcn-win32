/*
 * dlfcn-win32
 * Copyright (c) 2007-2009 Ramiro Polla
 * Copyright (c) 2014      Tiancheng "Timothy" Gu
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

#include <stdio.h>
#include "dlfcn.h"

/* If these dlclose's fails, we don't care as the handles are going to be
   closed eventually when the program ends. */
#define CLOSE_LIB    dlclose( library );
#define CLOSE_GLOBAL dlclose( global  );

#define RETURN_ERROR return 1;

/* This is what this test does:
 * - Open library with RTLD_GLOBAL
 * - Get global object
 * - Get symbol from library through library object <- works
 * - Run function if it worked
 * - Get symbol from library through global object  <- works
 * - Run function if it worked
 * - Close library
 * - Open library with RTLD_LOCAL
 * - Get symbol from library through library object <- works
 * - Run function if it worked
 * - Get symbol from library through global object  <- fails
 * - Run function if it worked
 * - Open library again (without closing it first) with RTLD_GLOBAL
 * - Get symbol from library through global object  <- works
 * - Close library
 * - Close global object
 */

int main()
{
    void *global;
    void *library;
    char *error;
    int (*function)( void );
    int ret;

    library = dlopen( "testdll.dll", RTLD_GLOBAL );
    if( !library )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library globally: %s\n", error ? error : "" );
    }
    else
        printf( "SUCCESS\tOpened library globally: %p\n", library );

    global = dlopen( 0, RTLD_GLOBAL );
    if( !global )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open global handle: %s\n", error ? error : "" );
        CLOSE_LIB
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tGot global handle: %p\n", global );

    function = dlsym( library, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from library handle: %s\n",
                error ? error : "" );
        CLOSE_LIB
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tGot symbol from library handle: %p\n", function );

    if( function )
        function( );

    function = dlsym( global, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from global handle: %s\n",
                error ? error : "" );
        CLOSE_LIB
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", function );

    if( function )
        function( );

    ret = dlclose( library );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close library: %s\n", error ? error : "" );
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tClosed library.\n" );

    library = dlopen( "testdll.dll", RTLD_LOCAL );
    if( !library )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library locally: %s\n", error ? error : "" );
        CLOSE_LIB
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tOpened library locally: %p\n", library );

    function = dlsym( library, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from library handle: %s\n",
                error ? error : "" );
        CLOSE_LIB
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tGot symbol from library handle: %p\n", function );

    if( function )
        function( );

    function = dlsym( global, "function" );
    if( function )
    {
        error = dlerror( );
        printf( "ERROR\tGot local symbol from global handle: %s\n",
                error ? error : "" );
        CLOSE_LIB
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tDid not get local symbol from global handle: %p\n", function );

    if( function )
        function( );

    ret = dlclose( library );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close library: %s\n", error ? error : "" );
        CLOSE_GLOBAL
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tClosed library.\n" );

    ret = dlclose( global );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close global handle: %s\n", error ? error : "" );
        RETURN_ERROR
    }
    else
        printf( "SUCCESS\tClosed global handle.\n" );

    return 0;
}
