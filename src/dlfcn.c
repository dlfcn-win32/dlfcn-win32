/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 * Copyright (c) 2015 Tiancheng "Timothy" Gu
 * Copyright (c) 2019 Pali Rohár <pali.rohar@gmail.com>
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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
/* https://docs.microsoft.com/en-us/cpp/intrinsics/returnaddress */
#pragma intrinsic(_ReturnAddress)
#else
/* https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html */
#ifndef _ReturnAddress
#define _ReturnAddress() (__builtin_extract_return_addr(__builtin_return_address(0)))
#endif
#endif

#ifdef SHARED
#define DLFCN_WIN32_EXPORTS
#endif
#include "dlfcn.h"

/* Note:
 * MSDN says these functions are not thread-safe. We make no efforts to have
 * any kind of thread safety.
 */

typedef struct local_object {
    HMODULE hModule;
    struct local_object *previous;
    struct local_object *next;
} local_object;

static local_object first_object;

/* These functions implement a double linked list for the local objects. */
static local_object *local_search( HMODULE hModule )
{
    local_object *pobject;

    if( hModule == NULL )
        return NULL;

    for( pobject = &first_object; pobject; pobject = pobject->next )
        if( pobject->hModule == hModule )
            return pobject;

    return NULL;
}

static BOOL local_add( HMODULE hModule )
{
    local_object *pobject;
    local_object *nobject;

    if( hModule == NULL )
        return TRUE;

    pobject = local_search( hModule );

    /* Do not add object again if it's already on the list */
    if( pobject )
        return TRUE;

    for( pobject = &first_object; pobject->next; pobject = pobject->next );

    nobject = (local_object*) malloc( sizeof( local_object ) );

    if( !nobject )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    pobject->next = nobject;
    nobject->next = NULL;
    nobject->previous = pobject;
    nobject->hModule = hModule;

    return TRUE;
}

static void local_rem( HMODULE hModule )
{
    local_object *pobject;

    if( hModule == NULL )
        return;

    pobject = local_search( hModule );

    if( !pobject )
        return;

    if( pobject->next )
        pobject->next->previous = pobject->previous;
    if( pobject->previous )
        pobject->previous->next = pobject->next;

    free( pobject );
}

/* POSIX says dlerror( ) doesn't have to be thread-safe, so we use one
 * static buffer.
 * MSDN says the buffer cannot be larger than 64K bytes, so we set it to
 * the limit.
 */
static char error_buffer[65535];
static BOOL error_occurred;

static void save_err_str( const char *str )
{
    DWORD dwMessageId;
    DWORD ret;
    size_t pos, len;

    dwMessageId = GetLastError( );

    if( dwMessageId == 0 )
        return;

    len = strlen( str );
    if( len > sizeof( error_buffer ) - 5 )
        len = sizeof( error_buffer ) - 5;

    /* Format error message to:
     * "<argument to function that failed>": <Windows localized error message>
      */
    pos = 0;
    error_buffer[pos++] = '"';
    memcpy( error_buffer+pos, str, len );
    pos += len;
    error_buffer[pos++] = '"';
    error_buffer[pos++] = ':';
    error_buffer[pos++] = ' ';

    ret = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwMessageId,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        error_buffer+pos, (DWORD) (sizeof(error_buffer)-pos), NULL );
    pos += ret;

    /* When FormatMessageA() fails it returns zero and does not touch buffer
     * so add trailing null byte */
    if( ret == 0 )
        error_buffer[pos] = '\0';

    if( pos > 1 )
    {
        /* POSIX says the string must not have trailing <newline> */
        if( error_buffer[pos-2] == '\r' && error_buffer[pos-1] == '\n' )
            error_buffer[pos-2] = '\0';
    }

    error_occurred = TRUE;
}

static void save_err_ptr_str( const void *ptr )
{
    char ptr_buf[19]; /* 0x<pointer> up to 64 bits. */

#ifdef _MSC_VER
/* Supress warning C4996: 'sprintf': This function or variable may be unsafe */
#pragma warning( suppress: 4996 )
#endif
    sprintf( ptr_buf, "0x%p", ptr );

    save_err_str( ptr_buf );
}

/* Load Psapi.dll at runtime, this avoids linking caveat */
static BOOL MyEnumProcessModules( HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded )
{
    static BOOL (WINAPI *EnumProcessModulesPtr)(HANDLE, HMODULE *, DWORD, LPDWORD);
    HMODULE psapi;

    if( !EnumProcessModulesPtr )
    {
        psapi = LoadLibraryA( "Psapi.dll" );
        if( psapi )
            EnumProcessModulesPtr = (BOOL (WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD)) GetProcAddress( psapi, "EnumProcessModules" );
        if( !EnumProcessModulesPtr )
            return 0;
    }

    return EnumProcessModulesPtr( hProcess, lphModule, cb, lpcbNeeded );
}

void *dlopen( const char *file, int mode )
{
    HMODULE hModule;
    UINT uMode;

    error_occurred = FALSE;

    /* Do not let Windows display the critical-error-handler message box */
    uMode = SetErrorMode( SEM_FAILCRITICALERRORS );

    if( file == 0 )
    {
        /* POSIX says that if the value of file is 0, a handle on a global
         * symbol object must be provided. That object must be able to access
         * all symbols from the original program file, and any objects loaded
         * with the RTLD_GLOBAL flag.
         * The return value from GetModuleHandle( ) allows us to retrieve
         * symbols only from the original program file. EnumProcessModules() is
         * used to access symbols from other libraries. For objects loaded
         * with the RTLD_LOCAL flag, we create our own list later on. They are
         * excluded from EnumProcessModules() iteration.
         */
        hModule = GetModuleHandle( NULL );

        if( !hModule )
            save_err_str( "(null)" );
    }
    else
    {
        HANDLE hCurrentProc;
        DWORD dwProcModsBefore, dwProcModsAfter;
        char lpFileName[MAX_PATH];
        size_t i, len;

        len = strlen( file );

        if( len >= sizeof( lpFileName ) )
        {
            SetLastError( ERROR_FILENAME_EXCED_RANGE );
            save_err_str( file );
            hModule = NULL;
        }
        else
        {
            /* MSDN says backslashes *must* be used instead of forward slashes. */
            for( i = 0; i < len; i++ )
            {
                if( file[i] == '/' )
                    lpFileName[i] = '\\';
                else
                    lpFileName[i] = file[i];
            }
            lpFileName[len] = '\0';

            hCurrentProc = GetCurrentProcess( );

            if( MyEnumProcessModules( hCurrentProc, NULL, 0, &dwProcModsBefore ) == 0 )
                dwProcModsBefore = 0;

            /* POSIX says the search path is implementation-defined.
             * LOAD_WITH_ALTERED_SEARCH_PATH is used to make it behave more closely
             * to UNIX's search paths (start with system folders instead of current
             * folder).
             */
            hModule = LoadLibraryExA( lpFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

            if( !hModule )
            {
                save_err_str( lpFileName );
            }
            else
            {
                if( MyEnumProcessModules( hCurrentProc, NULL, 0, &dwProcModsAfter ) == 0 )
                    dwProcModsAfter = 0;

                /* If the object was loaded with RTLD_LOCAL, add it to list of local
                 * objects, so that its symbols cannot be retrieved even if the handle for
                 * the original program file is passed. POSIX says that if the same
                 * file is specified in multiple invocations, and any of them are
                 * RTLD_GLOBAL, even if any further invocations use RTLD_LOCAL, the
                 * symbols will remain global. If number of loaded modules was not
                 * changed after calling LoadLibraryEx(), it means that library was
                 * already loaded.
                 */
                if( (mode & RTLD_LOCAL) && dwProcModsBefore != dwProcModsAfter )
                {
                    if( !local_add( hModule ) )
                    {
                        save_err_str( lpFileName );
                        FreeLibrary( hModule );
                        hModule = NULL;
                    }
                }
                else if( !(mode & RTLD_LOCAL) && dwProcModsBefore == dwProcModsAfter )
                {
                    local_rem( hModule );
                }
            }
        }
    }

    /* Return to previous state of the error-mode bit flags. */
    SetErrorMode( uMode );

    return (void *) hModule;
}

int dlclose( void *handle )
{
    HMODULE hModule = (HMODULE) handle;
    BOOL ret;

    error_occurred = FALSE;

    ret = FreeLibrary( hModule );

    /* If the object was loaded with RTLD_LOCAL, remove it from list of local
     * objects.
     */
    if( ret )
        local_rem( hModule );
    else
        save_err_ptr_str( handle );

    /* dlclose's return value in inverted in relation to FreeLibrary's. */
    ret = !ret;

    return (int) ret;
}

__declspec(noinline) /* Needed for _ReturnAddress() */
void *dlsym( void *handle, const char *name )
{
    FARPROC symbol;
    HMODULE hCaller;
    HMODULE hModule;
    HANDLE hCurrentProc;

    error_occurred = FALSE;

    symbol = NULL;
    hCaller = NULL;
    hModule = GetModuleHandle( NULL );
    hCurrentProc = GetCurrentProcess( );

    if( handle == RTLD_DEFAULT )
    {
        /* The symbol lookup happens in the normal global scope; that is,
         * a search for a symbol using this handle would find the same
         * definition as a direct use of this symbol in the program code.
         * So use same lookup procedure as when filename is NULL.
         */
        handle = hModule;
    }
    else if( handle == RTLD_NEXT )
    {
        /* Specifies the next object after this one that defines name.
         * This one refers to the object containing the invocation of dlsym().
         * The next object is the one found upon the application of a load
         * order symbol resolution algorithm. To get caller function of dlsym()
         * use _ReturnAddress() intrinsic. To get HMODULE of caller function
         * use standard GetModuleHandleExA() function.
         */
        if( !GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) _ReturnAddress( ), &hCaller ) )
            goto end;
    }

    if( handle != RTLD_NEXT )
    {
        symbol = GetProcAddress( (HMODULE) handle, name );

        if( symbol != NULL )
            goto end;
    }

    /* If the handle for the original program file is passed, also search
     * in all globally loaded objects.
     */

    if( hModule == handle || handle == RTLD_NEXT )
    {
        HMODULE *modules;
        DWORD cbNeeded;
        DWORD dwSize;
        size_t i;

        /* GetModuleHandle( NULL ) only returns the current program file. So
         * if we want to get ALL loaded module including those in linked DLLs,
         * we have to use EnumProcessModules( ).
         */
        if( MyEnumProcessModules( hCurrentProc, NULL, 0, &dwSize ) != 0 )
        {
            modules = malloc( dwSize );
            if( modules )
            {
                if( MyEnumProcessModules( hCurrentProc, modules, dwSize, &cbNeeded ) != 0 && dwSize == cbNeeded )
                {
                    for( i = 0; i < dwSize / sizeof( HMODULE ); i++ )
                    {
                        if( handle == RTLD_NEXT && hCaller )
                        {
                            /* Next modules can be used for RTLD_NEXT */
                            if( hCaller == modules[i] )
                                hCaller = NULL;
                            continue;
                        }
                        if( local_search( modules[i] ) )
                            continue;
                        symbol = GetProcAddress( modules[i], name );
                        if( symbol != NULL )
                        {
                            free( modules );
                            goto end;
                        }
                    }

                }
                free( modules );
            }
            else
            {
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto end;
            }
        }
    }

end:
    if( symbol == NULL )
    {
        if( GetLastError() == 0 )
            SetLastError( ERROR_PROC_NOT_FOUND );
        save_err_str( name );
    }

    return *(void **) (&symbol);
}

char *dlerror( void )
{
    /* If this is the second consecutive call to dlerror, return NULL */
    if( !error_occurred )
        return NULL;

    /* POSIX says that invoking dlerror( ) a second time, immediately following
     * a prior invocation, shall result in NULL being returned.
     */
    error_occurred = FALSE;

    return error_buffer;
}

#ifdef SHARED
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
    (void) hinstDLL;
    (void) fdwReason;
    (void) lpvReserved;
    return TRUE;
}
#endif
