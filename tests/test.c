/*
 * dlfcn-win32
 * Copyright (c) 2007-2009 Ramiro Polla
 * Copyright (c) 2014      Tiancheng "Timothy" Gu
 * Copyright (c) 2019      Pali Rohár <pali.rohar@gmail.com>
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

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include "dlfcn.h"

/* If these dlclose's fails, we don't care as the handles are going to be
   closed eventually when the program ends. */
#define CLOSE_LIB    dlclose( library )
#define CLOSE_GLOBAL dlclose( global  )

#define RETURN_ERROR printf("From line %d\n", __LINE__); return 1

#define RUNFUNC do { \
                    ret = function (); \
                    if( ret != 0) {    \
                        CLOSE_LIB;     \
                        CLOSE_GLOBAL;  \
                        RETURN_ERROR;  \
                    }                  \
                } while( 0 )

/* This is what this test does:
 * - Open library with RTLD_GLOBAL
 * - Get global object
 * - Get symbol from library through library object <- works
 * - Run function if it worked
 * - Get nonexistent symbol from library through library object <- fails
 * - Get symbol from library through global object  <- works
 * - Run function if it worked
 * - Get nonexistent symbol from library through global object <- fails
 * - Close library
 * - Open library with RTLD_LOCAL
 * - Get symbol from library through library object <- works
 * - Run function if it worked
 * - Get nonexistent symbol from library through library object <- fails
 * - Get local symbol from library through global object  <- fails
 * - Get nonexistent local symbol from library through global object <- fails
 * - Open library again (without closing it first) with RTLD_GLOBAL
 * - Get symbol from library through global object  <- works
 * - Get nonexistent symbol from library through global object <- fails
 * - Close library
 * - Close global object
 *
 * If one test fails, the program terminates itself.
 */

int main()
{
    void *global;
    void *library2;
    void *library;
    char *error;
    int (*function)( void );
    int (*function2_from_library2)( void );
    size_t (*fwrite_local) ( const void *, size_t, size_t, FILE * );
    size_t (*fputs_default) ( const char *, FILE * );
    int (*nonexistentfunction)( void );
    int fd;
    int ret;
    HMODULE library3;
    char toolongfile[32767];
    DWORD code;
    char nonlibraryfile[MAX_PATH];
    char bytename[sizeof("testdll2.dll")+4];
    WCHAR widename[sizeof("testdll2.dll")+1];
    DWORD length;
    HANDLE tempfile;
    DWORD dummy;
    UINT uMode;

#ifdef _DEBUG
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
#endif

    length = GetTempPathA( sizeof( nonlibraryfile ) - sizeof( "temp.dll" ), nonlibraryfile );
    if( length == 0 || length > sizeof( nonlibraryfile ) - sizeof( "temp.dll" ) )
    {
        printf( "ERROR\tGetTempPath failed\n" );
        RETURN_ERROR;
    }

    memcpy( nonlibraryfile + length, "temp.dll", sizeof( "temp.dll" ) );

    tempfile = CreateFileA( (LPCSTR) nonlibraryfile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    if( tempfile == INVALID_HANDLE_VALUE )
    {
        printf( "ERROR\tCannot create temporary file %s: %lu\n", nonlibraryfile, (unsigned long)GetLastError( ) );
        RETURN_ERROR;
    }

    WriteFile( tempfile, "test content", 12, &dummy, NULL );

    CloseHandle( tempfile );

    uMode = SetErrorMode( SEM_FAILCRITICALERRORS );
    library3 = LoadLibraryA( nonlibraryfile );
    code = GetLastError( );
    SetErrorMode( uMode );
    if( library3 )
    {
        printf( "ERROR\tNon-library file %s was opened via WINAPI\n", nonlibraryfile );
        CloseHandle( library3 );
        DeleteFileA( nonlibraryfile );
        RETURN_ERROR;
    }
    else if( code != ERROR_BAD_EXE_FORMAT )
    {
        printf( "ERROR\tNon-library file %s was processed via WINAPI: %lu\n", nonlibraryfile, (unsigned long)code );
        DeleteFileA( nonlibraryfile );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not open non-library file %s via WINAPI: %lu\n", nonlibraryfile, (unsigned long)code );

    library = dlopen( nonlibraryfile, RTLD_GLOBAL );
    if( library )
    {
        printf( "ERROR\tNon-library file %s was opened via dlopen\n", nonlibraryfile );
        dlclose( library );
        DeleteFileA( nonlibraryfile );
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlopen for non-library file\n" );
        DeleteFileA( nonlibraryfile );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not open non-library file %s: %s\n", nonlibraryfile, error );

    DeleteFileA( nonlibraryfile );

    library = dlopen( "nonexistentfile.dll", RTLD_GLOBAL );
    if( library )
    {
        printf( "ERROR\tNon-existent file nonexistentfile.dll was opened via dlopen\n" );
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlopen for non-existent file\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not open non-existent file nonexistentfile.dll: %s\n", error );

    memset( toolongfile, 'X', sizeof( toolongfile ) - 5 );
    memcpy( toolongfile + sizeof( toolongfile ) - 5, ".dll", 5 );

    library = dlopen( toolongfile, RTLD_GLOBAL );
    if( library )
    {
        printf( "ERROR\tFile with too long file name was opened via dlopen\n" );
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlopen for file with too long file name\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not open file with too long file name: %s\n", error );

    uMode = SetErrorMode( SEM_FAILCRITICALERRORS );
    library3 = LoadLibraryA( toolongfile );
    code = GetLastError( );
    SetErrorMode( uMode );
    if( library3 )
    {
        printf( "ERROR\tFile with too long file name was opened via WINAPI\n" );
        RETURN_ERROR;
    }
    else if( code != ERROR_FILENAME_EXCED_RANGE )
    {
        printf( "ERROR\tFile with too long file name was processed via WINAPI: %lu\n", (unsigned long)code );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not open file with too long file name via WINAPI: %lu\n", (unsigned long)code );

    fd = _open( "testdll2.dll", _O_RDONLY | _O_BINARY );
    if( fd < 0 )
    {
        if( _chdir( "Debug" ) == 0 )
        {
            fd = _open( "testdll2.dll", _O_RDONLY | _O_BINARY );
            if( fd < 0 )
                _chdir( ".." );
        }
    }
    if( fd < 0 )
    {
        if( _chdir( "Release" ) == 0 )
        {
            fd = _open( "testdll2.dll", _O_RDONLY | _O_BINARY );
            if( fd < 0 )
                _chdir( ".." );
        }
    }
    if( fd < 0 )
    {
        printf( "ERROR\tCould not open library2 file: %s\n", strerror( errno ) );
        RETURN_ERROR;
    }
    _close( fd );

    library2 = dlopen( "testdll2.dll", RTLD_GLOBAL );
    if( !library2 )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library2 globally: %s\n", error ? error : "" );
        RETURN_ERROR; 
    }
    else
        printf( "SUCCESS\tOpened library2 globally: %p\n", library2 );

    widename[0] = 0xE1; /* wide non-ASCII character mappable to most ANSI codepages */
    wcscpy( widename+1, L"testdll2.dll" );
    if( !CopyFileW( L"testdll2.dll", widename, FALSE ))
    {
        printf( "ERROR\tCould not copy file testdll2.dll: %lu\n", (unsigned long)GetLastError( ) );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCopied testdll2.dll to wide name %ls\n", widename );

    ret = WideCharToMultiByte( CP_ACP, 0, widename, -1, bytename, sizeof( bytename ), NULL, NULL );
    /* if we cannot convert widename to current codepage (used by _open() function), skip this test */
    if( ret > 0 )
    {
        bytename[ret] = 0;

        fd = _wopen( widename, _O_RDONLY | _O_BINARY );
        if( fd < 0 )
        {
            printf( "ERROR\tCould not open copied wide library2 file %ls: %s\n", widename, strerror( errno ) );
            DeleteFileW( widename );
            RETURN_ERROR;
        }
        _close( fd );

        fd = _open( bytename, _O_RDONLY | _O_BINARY );
        if( fd < 0 )
        {
            printf( "ERROR\tCould not open copied wide library2 file %s: %s\n", bytename, strerror( errno ) );
            DeleteFileW( widename );
            RETURN_ERROR;
        }
        _close( fd );

        dlclose( library2 );
        library2 = dlopen( bytename, RTLD_GLOBAL );
        if( !library2 )
        {
            error = dlerror( );
            printf( "ERROR\tCould not open copied wide library2 file %s globally: %s\n", bytename, error ? error : "" );
            DeleteFileW( widename );
            RETURN_ERROR;
        }
        else
            printf( "SUCCESS\tOpened copied wide library2 file %s globally: %p\n", bytename, library2 );

        dlclose( library2 );
        DeleteFileW( widename );

        library2 = dlopen( "testdll2.dll", RTLD_GLOBAL );
        if( !library2 )
        {
            error = dlerror( );
            printf( "ERROR\tCould not open library2 globally: %s\n", error ? error : "" );
            RETURN_ERROR;
        }
        else
            printf( "SUCCESS\tOpened library2 globally: %p\n", library2 );
    }

    library = dlopen( "testdll.dll", RTLD_GLOBAL );
    if( !library )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library globally: %s\n", error ? error : "" );
        RETURN_ERROR; 
    }
    else
        printf( "SUCCESS\tOpened library globally: %p\n", library );

    global = dlopen( 0, RTLD_GLOBAL );
    if( !global )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open global handle: %s\n", error ? error : "" );
        CLOSE_LIB;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot global handle: %p\n", global );

    *(void **) (&fwrite_local) = dlsym( global, "fwrite" );
    if (!fwrite_local)
    {
        error = dlerror();
        printf("ERROR\tCould not get symbol from global handle: %s\n",
            error ? error : "");
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", *(void **) (&fwrite_local) );
    {
        const char *hello_world = "Hello world from local fwrite!\n";
        fwrite_local( hello_world, sizeof( char ), strlen( hello_world ), stderr );
        fflush( stderr );
    }

    *(void **) (&fputs_default) = dlsym( RTLD_DEFAULT, "fputs" );
    if (!fputs_default)
    {
        error = dlerror();
        printf("ERROR\tCould not get symbol from default handle: %s\n",
            error ? error : "");
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from default handle: %p\n", *(void **) (&fputs_default) );
    {
        const char *hello_world_fputs = "Hello world from default fputs!\n";
        fputs_default( hello_world_fputs, stderr );
        fflush( stderr );
    }

    *(void **) (&function) = dlsym( library, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from library handle: %s\n",
                error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from library handle: %p\n", *(void **) (&function) );

    RUNFUNC;

    *(void **) (&function2_from_library2) = dlsym( library2, "function2" );
    if( !function2_from_library2 )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from library2 handle: %s\n",
                error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from library2 handle: %p\n", *(void **) (&function2_from_library2) );

    ret = function2_from_library2 ();
    if( ret != 2 )
    {
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }

    *(void **) (&nonexistentfunction) = dlsym( library, "nonexistentfunction" );
    if( nonexistentfunction )
    {
        error = dlerror( );
        printf( "ERROR\tGot nonexistent symbol from library handle: %p\n", *(void **) (&nonexistentfunction) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlsym for nonexistent symbol\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not get nonexistent symbol from library handle: %s\n", error );

    *(void **) (&function) = dlsym( global, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from global handle: %s\n",
                error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", *(void **) (&function) );

    RUNFUNC;

    *(void **) (&nonexistentfunction) = dlsym( global, "nonexistentfunction" );
    if( nonexistentfunction )
    {
        error = dlerror( );
        printf( "ERROR\tGot nonexistent symbol from global handle: %p\n", *(void **) (&nonexistentfunction) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlsym for nonexistent symbol\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not get nonexistent symbol from global handle: %s\n", error );

    ret = dlclose( library );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close library: %s\n", error ? error : "" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tClosed library.\n" );

    ret = dlclose( library2 );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close library2: %s\n", error ? error : "" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tClosed library2.\n" );

    library = dlopen( "testdll.dll", RTLD_LOCAL );
    if( !library )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library locally: %s\n", error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tOpened library locally: %p\n", library );

    *(void **) (&function) = dlsym( library, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from library handle: %s\n",
                error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from library handle: %p\n", *(void **) (&function) );

    RUNFUNC;

    *(void **) (&nonexistentfunction) = dlsym( library, "nonexistentfunction" );
    if( nonexistentfunction )
    {
        error = dlerror( );
        printf( "ERROR\tGot nonexistent symbol from library handle: %p\n", *(void **) (&nonexistentfunction) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlsym for nonexistent symbol\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tCould not get nonexistent symbol from library handle: %s\n", error );

    *(void **) (&function) = dlsym( global, "function" );
    if( function )
    {
        error = dlerror( );
        printf( "ERROR\tGot local symbol from global handle: %s @ %p\n",
                error ? error : "", *(void **) (&function) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tDid not get local symbol from global handle.\n" );

    *(void **) (&nonexistentfunction) = dlsym( global, "nonexistentfunction" );
    if( nonexistentfunction )
    {
        error = dlerror( );
        printf( "ERROR\tGot nonexistent local symbol from global handle: %p\n", *(void **) (&nonexistentfunction) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlsym for nonexistent symbol\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tDid not get nonexistent local symbol from global handle: %s\n", error );

    library = dlopen( "testdll.dll", RTLD_GLOBAL );
    if( !library )
    {
        error = dlerror( );
        printf( "ERROR\tCould not open library globally without closing it first: %s\n", error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tOpened library globally without closing it first: %p\n", library );

    *(void **) (&function) = dlsym( global, "function" );
    if( !function )
    {
        error = dlerror( );
        printf( "ERROR\tCould not get symbol from global handle: %s\n",
                error ? error : "" );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", *(void **) (&function) );

    RUNFUNC;

    *(void **) (&nonexistentfunction) = dlsym( global, "nonexistentfunction" );
    if( nonexistentfunction )
    {
        error = dlerror( );
        printf( "ERROR\tGot nonexistent symbol from global handle: %p\n", *(void **) (&nonexistentfunction) );
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    error = dlerror( );
    if( !error )
    {
        printf( "ERROR\tNo error from dlsym for nonexistent symbol\n" );
        RETURN_ERROR;
    }
    else
    {
        printf( "SUCCESS\tCould not get nonexistent symbol from global handle: %s\n", error );
                
        /* Test that the second call to dlerror() returns null as in the specs 
           See https://github.com/dlfcn-win32/dlfcn-win32/issues/34 */
        error = dlerror( );
        if( error == NULL )
        {
            printf( "SUCCESS\tSecond consecutive call to dlerror returned NULL\n");
        }
        else 
        {
            printf( "ERROR\tSecond consecutive call to dlerror returned a non-NULL pointer: %p\n", error );
            CLOSE_LIB;
            CLOSE_GLOBAL;
            RETURN_ERROR;
        }
    }

    *(void **) (&function) = dlsym( global, "fwrite" );
    if (!function)
    {
        error = dlerror();
        printf("ERROR\tCould not get symbol from global handle: %s\n",
            error ? error : "");
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", *(void **) (&function) );
    

    uMode = SetErrorMode( SEM_FAILCRITICALERRORS );
    library3 = LoadLibraryA("testdll3.dll");
    SetErrorMode( uMode );
    if (!library3)
    {
        printf( "ERROR\tCould not open library3 via WINAPI\n" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tOpened library3 via WINAPI: %p\n", (void *)library3 );

    ret = dlclose( library );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close library: %s\n", error ? error : "" );
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tClosed library.\n" );

    *(void **) (&function) = dlsym( global, "function3" );
    if (!function)
    {
        error = dlerror();
        printf("ERROR\tCould not get symbol from global handle: %s\n",
            error ? error : "");
        CLOSE_LIB;
        CLOSE_GLOBAL;
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tGot symbol from global handle: %p\n", *(void **) (&function) );

    RUNFUNC;

    ret = dlclose( global );
    if( ret )
    {
        error = dlerror( );
        printf( "ERROR\tCould not close global handle: %s\n", error ? error : "" );
        RETURN_ERROR;
    }
    else
        printf( "SUCCESS\tClosed global handle.\n" );

#ifdef _DEBUG
    _CrtDumpMemoryLeaks();
#endif
    return 0;
}
