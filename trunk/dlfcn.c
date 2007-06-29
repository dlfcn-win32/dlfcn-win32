/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <windows.h>
#include <stdio.h>

#include "dlfcn.h"

/* Note:
 * MSDN says these functions are not thread-safe. We make no efforts to have
 * any kind of thread safety.
 */

/* I have no special reason to have set MAX_GLOBAL_OBJECTS to this value. Any
 * comments are welcome.
 */
#define MAX_OBJECTS 255

static HMODULE global_objects[MAX_OBJECTS];

/* This function adds an object to the list of global objects.
 * The implementation is very simple and slow.
 * TODO: should failing this function be enough to fail the call to dlopen( )?
 */
static void global_object_add( HMODULE hModule )
{
    int i;

    for( i = 0 ; i < MAX_OBJECTS ; i++ )
    {
        if( !global_objects[i] )
        {
            global_objects[i] = hModule;
            break;
        }
    }
}

static void global_object_rem( HMODULE hModule )
{
    int i;

    for( i = 0 ; i < MAX_OBJECTS ; i++ )
    {
        if( global_objects[i] == hModule )
        {
            global_objects[i] = 0;
            break;
        }
    }
}

/* POSIX says dlerror( ) doesn't have to be thread-safe, so we use one
 * static buffer.
 * MSDN says the buffer cannot be larger than 64K bytes, so we set it to
 * the limit.
 */
static char error_buffer[65535];
static int dlerror_was_last_call;

static int copy_string( char *dest, int dest_size, const char *src )
{
    int i = 0;

    /* gcc should optimize this out */
    if( !src && !dest )
        return 0;

    for( i = 0 ; i < dest_size-1 ; i++ )
    {
        if( !src[i] )
            break;
        else
            dest[i] = src[i];
    }
    dest[i] = '\0';

    return i;
}

static void save_err_str( const char *str )
{
    DWORD dwMessageId;
    DWORD pos;

    dwMessageId = GetLastError( );

    if( dwMessageId == 0 )
        return;

    /* Format error message to:
     * "<argument to function that failed>": <Windows localized error message>
     */
    pos  = copy_string( error_buffer,     sizeof(error_buffer),     "\"" );
    pos += copy_string( error_buffer+pos, sizeof(error_buffer)-pos, str );
    pos += copy_string( error_buffer+pos, sizeof(error_buffer)-pos, "\": " );
    pos += FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwMessageId,
                          MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                          error_buffer+pos, sizeof(error_buffer)-pos, NULL );

    if( pos > 1 )
    {
        /* POSIX says the string must not have trailing <newline> */
        if( error_buffer[pos-2] == '\r' && error_buffer[pos-1] == '\n' )
            error_buffer[pos-2] = '\0';
    }
}

static void save_err_ptr( const void *ptr )
{
    char ptr_buf[19]; /* 0x<pointer> up to 64 bits. */

    sprintf( ptr_buf, "0x%p", ptr );

    save_err_str( ptr_buf );
}

void *dlopen( const char *file, int mode )
{
    HMODULE hModule;
    UINT uMode;

    dlerror_was_last_call = 0;

    /* Do not let Windows display the critical-error-handler message box */
    uMode = SetErrorMode( SEM_FAILCRITICALERRORS );

    if( file == 0 )
    {
        /* POSIX says that if the value of file is 0, a handle on a global
         * symbol object must be provided. That object must be able to access
         * all symbols from the original program file, and any objects loaded
         * with the RTLD_GLOBAL flag.
         * The return value from GetModuleHandle( ) allows us to retrieve
         * symbols only from the original program file. For objects loaded with
         * the RTLD_GLOBAL flag, we create our own list later on.
         */
        hModule = GetModuleHandle( NULL );

        if( !hModule )
            save_err_ptr( file );
    }
    else
    {
        char lpFileName[MAX_PATH];
        int i;

        /* MSDN says backslashes *must* be used instead of forward slashes. */
        for( i = 0 ; i < sizeof(lpFileName)-1 ; i++ )
        {
            if( !file[i] )
                break;
            else if( file[i] == '/' )
                lpFileName[i] = '\\';
            else
                lpFileName[i] = file[i];
        }
        lpFileName[i] = '\0';

        /* POSIX says the search path is implementation-defined.
         * LOAD_WITH_ALTERED_SEARCH_PATH is used to make it behave more closely
         * to UNIX's search paths (start with system folders instead of current
         * folder).
         */
        hModule = LoadLibraryEx( (LPSTR) lpFileName, NULL, 
                                 LOAD_WITH_ALTERED_SEARCH_PATH );

        /* If the object was loaded with RTLD_GLOBAL, add it to list of global
         * objects, so that its symbols may be retrieved even if the handle for
         * the original program file is passed. POSIX says that if the same
         * file is specified in multiple invocations, and any of them are
         * RTLD_GLOBAL, even if any further invocations use RTLD_LOCAL, the
         * symbols will remain global.
         */
        if( !hModule )
            save_err_str( lpFileName );
        else if( (mode & RTLD_GLOBAL) )
            global_object_add( hModule );
    }

    /* Return to previous state of the error-mode bit flags. */
    SetErrorMode( uMode );

    return (void *) hModule;
}

int dlclose( void *handle )
{
    HMODULE hModule = (HMODULE) handle;
    BOOL ret;

    dlerror_was_last_call = 0;

    ret = FreeLibrary( hModule );

    /* If the object was loaded with RTLD_GLOBAL, remove it from list of global
     * objects.
     */
    if( ret )
        global_object_rem( hModule );
    else
        save_err_ptr( handle );

    /* dlclose's return value in inverted in relation to FreeLibrary's. */
    ret = !ret;

    return (int) ret;
}

void *dlsym( void *handle, const char *name )
{
    FARPROC symbol;

    dlerror_was_last_call = 0;

    symbol = GetProcAddress( handle, name );

    if( symbol == NULL )
    {
        HMODULE hModule;

        /* If the handle for the original program file is passed, also search
         * in all globally loaded objects.
         */

        hModule = GetModuleHandle( NULL );

        if( hModule == handle )
        {
            int i;

            for( i = 0 ; i < MAX_OBJECTS ; i++ )
            {
                if( global_objects[i] != 0 )
                {
                    symbol = GetProcAddress( global_objects[i], name );
                    if( symbol != NULL )
                        break;
                }
            }
        }

        CloseHandle( hModule );
    }

    if( symbol == NULL )
        save_err_str( name );

    return (void*) symbol;
}

char *dlerror( void )
{
    /* POSIX says that invoking dlerror( ) a second time, immediately following
     * a prior invocation, shall result in NULL being returned.
     */
    if( dlerror_was_last_call )
        return NULL;

    dlerror_was_last_call = 1;

    return error_buffer;
}
