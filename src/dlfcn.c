/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 * Copyright (c) 2015 Tiancheng "Timothy" Gu
 * Copyright (c) 2019 Pali Rohár <pali.rohar@gmail.com>
 * Copyright (c) 2020 Ralf Habacker <ralf.habacker@freenet.de>
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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

/* Older versions do not have this type */
#if _WIN32_WINNT < 0x0500
typedef ULONG ULONG_PTR;
#endif

/* Older SDK versions do not have these macros */
#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x4
#endif
#ifndef GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x2
#endif
#ifndef IMAGE_NT_OPTIONAL_HDR_MAGIC
#ifdef _WIN64
#define IMAGE_NT_OPTIONAL_HDR_MAGIC 0x20b
#else
#define IMAGE_NT_OPTIONAL_HDR_MAGIC 0x10b
#endif
#endif
#ifndef IMAGE_DIRECTORY_ENTRY_IAT
#define IMAGE_DIRECTORY_ENTRY_IAT 12
#endif
#ifndef LOAD_WITH_ALTERED_SEARCH_PATH
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x8
#endif

#ifdef _MSC_VER
#if _MSC_VER >= 1000
/* https://docs.microsoft.com/en-us/cpp/intrinsics/returnaddress */
#pragma intrinsic( _ReturnAddress )
#else
/* On older version read return address from the value on stack pointer + 4 of
 * the caller. Caller stack pointer is stored in EBP register but only when
 * the EBP register is not optimized out. Usage of _alloca() function prevent
 * EBP register optimization. Read value of EBP + 4 via inline assembly. And
 * because inline assembly does not have a return value, put it into naked
 * function which does not have prologue and epilogue and preserve registers.
 */
__declspec( naked ) static void *_ReturnAddress( void ) { __asm mov eax, [ebp+4] __asm ret }
#define _ReturnAddress( ) ( _alloca(1), _ReturnAddress( ) )
#endif
#else
/* https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html */
#ifndef _ReturnAddress
#define _ReturnAddress( ) ( __builtin_extract_return_addr( __builtin_return_address( 0 ) ) )
#endif
#endif

#ifdef DLFCN_WIN32_SHARED
#define DLFCN_WIN32_EXPORTS
#endif
#include "dlfcn.h"

#if defined( _MSC_VER ) && _MSC_VER >= 1300
/* https://docs.microsoft.com/en-us/cpp/cpp/noinline */
#define DLFCN_NOINLINE __declspec( noinline )
#elif defined( __GNUC__ ) && ( ( __GNUC__ > 3 ) || ( __GNUC__ == 3 && __GNUC_MINOR__ >= 1 ) )
/* https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html */
#define DLFCN_NOINLINE __attribute__(( noinline ))
#else
#define DLFCN_NOINLINE
#endif

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
    local_object *pobject = NULL;

    if( !hModule )
        return NULL;

    for( pobject = &first_object; pobject; pobject = pobject->next )
        if( pobject->hModule == hModule )
            return pobject;

    return NULL;
}

static BOOL local_add( HMODULE hModule )
{
    local_object *pobject = NULL;
    local_object *nobject = NULL;

    if( !hModule )
        return TRUE;

    pobject = local_search( hModule );

    /* Do not add object again if it's already on the list */
    if( pobject != NULL )
        return TRUE;

    for( pobject = &first_object; pobject->next; pobject = pobject->next );

    nobject = (local_object *) malloc( sizeof( local_object ) );

    if( !nobject )
        return FALSE;

    pobject->next = nobject;
    nobject->next = NULL;
    nobject->previous = pobject;
    nobject->hModule = hModule;
    return TRUE;
}

static void local_rem( HMODULE hModule )
{
    local_object *pobject = NULL;

    if( !hModule )
        return;

    pobject = local_search( hModule );

    if( pobject == NULL )
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
static char error_buffer[65535] = {0};
static BOOL error_occurred = FALSE;

static void save_err_str( const char *str, DWORD dwMessageId )
{
    DWORD ret;
    size_t pos, len;

    len = strlen( str );
    if( len > sizeof( error_buffer ) - 5 )
        len = sizeof( error_buffer ) - 5;

    /* Format error message to:
     * "<argument to function that failed>": <Windows localized error message>
      */
    pos = 0;
    error_buffer[pos++] = '"';
    memcpy( error_buffer + pos, str, len );
    pos += len;
    error_buffer[pos++] = '"';
    error_buffer[pos++] = ':';
    error_buffer[pos++] = ' ';

    ret = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwMessageId,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        error_buffer + pos, (DWORD) ( sizeof( error_buffer ) - pos ), NULL );
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

static void save_err_ptr_str( const void *ptr, DWORD dwMessageId )
{
    char ptr_buf[2 + 2 * sizeof( ptr ) + 1];
    char num;
    size_t i;

    ptr_buf[0] = '0';
    ptr_buf[1] = 'x';

    for( i = 0; i < 2 * sizeof( ptr ); i++ )
    {
        num = (char) ( ( ( (ULONG_PTR) ptr ) >> ( 8 * sizeof( ptr ) - 4 * ( i + 1 ) ) ) & 0xF );
        ptr_buf[2 + i] = num + ( ( num < 0xA ) ? '0' : ( 'A' - 0xA ) );
    }

    ptr_buf[2 + 2 * sizeof( ptr )] = 0;

    save_err_str( ptr_buf, dwMessageId );
}

typedef BOOL (WINAPI *SetThreadErrorModePtrCB)(DWORD, DWORD *);
BOOL WINAPI MySetThreadErrorMode(DWORD uMode, DWORD *oldMode )
{
    UINT mode = SetErrorMode( uMode );
    if ( oldMode )
        *oldMode = mode;
    return TRUE;
}
static SetThreadErrorModePtrCB SetThreadErrorModePtr = MySetThreadErrorMode;
static UINT WINAPI MySetErrorMode( UINT uMode )
{
    DWORD oldMode = 0;
    return (SetThreadErrorModePtr( uMode, &oldMode ) == FALSE) ? 0 : oldMode;
}

typedef DWORD (WINAPI *GetProcessIdCB)( HANDLE hProcess );
DWORD WINAPI FailGetProcessId( HANDLE hProcess )
{
	(void)hProcess;
	SetLastError( E_NOINTERFACE );
	return 0;
}
static GetProcessIdCB MyGetProcessId = NULL;

#include <tlhelp32.h>
typedef HANDLE (WINAPI *CreateToolhelp32SnapshotCB)( DWORD dwFlags, DWORD dwPid );
HANDLE WINAPI FailCreateToolhelp32Snapshot( DWORD dwFlags, DWORD dwPid )
{
	(void)dwFlags;
	(void)dwPid;
	SetLastError( E_NOINTERFACE );
	return INVALID_HANDLE_VALUE;
}
static CreateToolhelp32SnapshotCB MyCreateToolhelp32Snapshot = FailCreateToolhelp32Snapshot;

typedef BOOL (WINAPI *Module32NextCB)( HANDLE hSnapshot, LPMODULEENTRY32 lpme );
BOOL WINAPI FailModule32Next( HANDLE hSnapshot, LPMODULEENTRY32 lpme )
{
	(void)hSnapshot;
	(void)lpme;
	SetLastError( E_NOINTERFACE );
	return FALSE;
}
static Module32NextCB MyModule32First = FailModule32Next, MyModule32Next = FailModule32Next;

typedef BOOL (WINAPI *GetModuleHandleExAPtrCB)(DWORD, LPCSTR, HMODULE *);
BOOL WINAPI HackyGetModuleHandleExA
    ( DWORD dwFlags, LPCSTR lpModuleName, HMODULE *phModule )
{
    /* To get HMODULE from address use undocumented hack from https://stackoverflow.com/a/2396380
     * The HMODULE of a DLL is the same value as the module's base address.
     */
    MEMORY_BASIC_INFORMATION info;
    size_t sLen = VirtualQuery( lpModuleName, &info, sizeof( info ) );
    if( sLen != sizeof( info ) )
    {
        *phModule = NULL;
        return FALSE;
    }
    *phModule = (HMODULE) info.AllocationBase;
    return TRUE;
}
static GetModuleHandleExAPtrCB GetModuleHandleExAPtr = HackyGetModuleHandleExA;
static HMODULE WINAPI MyGetModuleHandleFromAddress( const void *addr )
{
    HMODULE hModule = NULL;
    /* If GetModuleHandleExA is available use it with GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS */
    if( !GetModuleHandleExAPtr( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, addr, &hModule ) )
        return NULL;
    return hModule;
}

/* Load Psapi.dll at runtime, this avoids linking caveat */
typedef BOOL (WINAPI *EnumProcessModulesPtrCB)(HANDLE, HMODULE *, DWORD, LPDWORD);
BOOL WINAPI Th32EnumProcessModules( HANDLE hProcess, HMODULE *hModules, DWORD cb, LPDWORD lpcbNeeded )
{
    DWORD i = 0, count = cb / sizeof(HMODULE);
    MODULEENTRY32 me = {0};
    HANDLE hShot = MyCreateToolhelp32Snapshot( TH32CS_SNAPMODULE, MyGetProcessId( hProcess ) );
    if ( hShot == INVALID_HANDLE_VALUE )
        return FALSE;
    me.dwSize = sizeof(me);
	for
	(
		BOOL cont = MyModule32First( hShot, &me )
		; cont == TRUE; cont = MyModule32Next( hShot, &me )
	)
	{
		if ( i < count )
			hModules[i] = me.hModule;
		++i;
	}
	CloseHandle( hShot );
	if ( lpcbNeeded ) *lpcbNeeded = (i * sizeof(HMODULE));
	return (i < count);
}
static EnumProcessModulesPtrCB MyEnumProcessModules = Th32EnumProcessModules;

DLFCN_EXPORT
void *dlopen( const char *file, int mode )
{
    HMODULE hModule = NULL;
    UINT uMode = 0;

    error_occurred = FALSE;

    /* Do not let Windows display the critical-error-handler message box */
    uMode = MySetErrorMode( SEM_FAILCRITICALERRORS );

    if( file == NULL )
    {
        /* POSIX says that if the value of file is NULL, a handle on a global
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
            save_err_str( "(null)", GetLastError( ) );
    }
    else
    {
        HANDLE hThisProc = NULL;
        DWORD dwProcModsBefore = 0, dwProcModsAfter = 0;
        char lpFileName[MAX_PATH] = {0};
        size_t i = 0, len = 0;

        len = strlen( file );

        if( len >= sizeof( lpFileName ) )
        {
            save_err_str( file, ERROR_FILENAME_EXCED_RANGE );
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

            hThisProc = GetCurrentProcess( );

            if( MyEnumProcessModules( hThisProc, NULL, 0, &dwProcModsBefore ) == 0 )
                dwProcModsBefore = 0;

            /* POSIX says the search path is implementation-defined.
             * LOAD_WITH_ALTERED_SEARCH_PATH is used to make it behave more closely
             * to UNIX's search paths (start with system folders instead of current
             * folder).
             */
            hModule = LoadLibraryExA( lpFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

            if( !hModule )
            {
                save_err_str( lpFileName, GetLastError( ) );
            }
            else
            {
                if( MyEnumProcessModules( hThisProc, NULL, 0, &dwProcModsAfter ) == 0 )
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
                        save_err_str( lpFileName, ERROR_NOT_ENOUGH_MEMORY );
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
    MySetErrorMode( uMode );

    return hModule;
}

DLFCN_EXPORT
int dlclose( void *hModule )
{
    BOOL ret = FreeLibrary( hModule );

    error_occurred = FALSE;

    /* If the object was loaded with RTLD_LOCAL, remove it from list of local
     * objects.
     */
    if( ret )
        local_rem( hModule );
    else
        save_err_ptr_str( hModule, GetLastError( ) );

    /* dlclose's return value in inverted in relation to FreeLibrary's. */
    ret = !ret;

    return (int) ret;
}

/* Named as such in case ever need to bring it to initial scope of dlsym */
typedef struct
{
    DWORD    cbHeapSize;
    HMODULE *hModules;
    HANDLE   hHeap;
} dlsym_vars;

#define TEST_WITH_MALLOC 0

static void dlsym_clear_heap( dlsym_vars *vars )
{
    if ( vars->hModules )
#if TEST_WITH_MALLOC
        free( vars->hModules );
#else
        HeapFree( vars->hHeap, 0, vars->hModules );
    HeapDestroy( vars->hHeap );
#endif
    vars->hModules = NULL;
    vars->hHeap = NULL;
}

static DWORD dlsym_alloc_heap( dlsym_vars *vars, DWORD cbNeed )
{
	DWORD dwMessageId = 0;

#if TEST_WITH_MALLOC
	vars->hModules = calloc(cbNeed,1);
#else
	/* We have to allocate more than we need because the spec says
	 * HeapAlloc cannot allocate the full heap to an allocation as it
	 * needs some of the memory for internal data, HeapCreate should've
	 * accounted for this with a prefix allocation but oh well.  */
	vars->hHeap = HeapCreate( 0, cbNeed * 2, cbNeed * 2 );
	if ( !(vars->hHeap) )
	{
		dwMessageId = GetLastError();
		return dwMessageId ? dwMessageId : ERROR_NOT_ENOUGH_MEMORY;
	}

	/* Using HeapAlloc() allows callers to use dlsym( RTLD_NEXT, "malloc" ) */
	vars->hModules = HeapAlloc( vars->hHeap, HEAP_ZERO_MEMORY, cbNeed );
#endif
	if ( !(vars->hModules) )
	{
		dwMessageId = GetLastError();
		dlsym_clear_heap( vars );
		return dwMessageId ? dwMessageId : ERROR_INVALID_TABLE;
	}

	vars->cbHeapSize = cbNeed;
	return 0;
}

DLFCN_NOINLINE /* Needed for _ReturnAddress() */
DLFCN_EXPORT
void *dlsym( void *handle, const char *name )
{
    dlsym_vars vars = {0};
    FARPROC symbol = NULL;
    HANDLE hThisProc = GetCurrentProcess( );
    DWORD dwMessageId = 0, cbNeeded = 0, i = 0, count = 0;
    HMODULE hCaller = NULL, hModule = GetModuleHandle( NULL ), hIter = NULL;

    error_occurred = 0;
    SetLastError(0);

    /* The symbol lookup happens in the normal global scope; that is,
    * a search for a symbol using this handle would find the same
    * definition as a direct use of this symbol in the program code.
    * So use same lookup procedure as when filename is NULL.
    */
    if ( handle == RTLD_NEXT )
    {
        /* Specifies the next object after this one that defines name.
         * This one refers to the object containing the invocation of dlsym().
         * The next object is the one found upon the application of a load
         * order symbol resolution algorithm. To get caller function of dlsym()
         * use _ReturnAddress() intrinsic. To get HMODULE of caller function
         * use MyGetModuleHandleFromAddress() which calls either standard
         * GetModuleHandleExA() function or hack via VirtualQuery().
         */
        hCaller = MyGetModuleHandleFromAddress( _ReturnAddress( ) );
        if( !hCaller )
        {
            dwMessageId = ERROR_INVALID_PARAMETER;
            goto end;
        }
    }
    else
    {
        if ( handle == RTLD_DEFAULT )
            handle = hModule;

        symbol = GetProcAddress( handle, name );

        if( symbol )
            goto end;
    }

    /* Just search always */

    /* If gonna make a call either way then might as well use this instead
     * of GetSystemInfo() for the pagesize */
    if ( !MyEnumProcessModules( hThisProc, NULL, 0, &cbNeeded ) || !cbNeeded )
    {
        dwMessageId = GetLastError();
        if ( !dwMessageId ) dwMessageId = ERROR_FUNCTION_FAILED;
        goto end;
    }

    while ( vars.cbHeapSize < cbNeeded )
    {
		dwMessageId = dlsym_alloc_heap( &vars, cbNeeded );
		if ( dwMessageId )
			goto end;
        /* GetModuleHandle( NULL ) only returns the current program file. So
         * if we want to get ALL loaded module including those in linked DLLs,
         * we have to use EnumProcessModules( ). */
        if ( !MyEnumProcessModules( hThisProc, vars.hModules, cbNeeded, &cbNeeded ) || !cbNeeded )
        {
            dwMessageId = GetLastError();
            if ( !dwMessageId ) dwMessageId = ERROR_CALLBACK_SUPPLIED_INVALID_DATA;
            goto freeHeap;
        }

        /* No big deal if what we got was less than the heap size so long as
         * we didn't need more than the heap size */
        if ( cbNeeded > vars.cbHeapSize )
            dlsym_clear_heap( &vars );
    }

    /* Realised my mistake here, we were indeed supposed to search from
     * bottom of the array since that's where the top level wrappers of
     * functions would lie. */
    for( i = 0, count = cbNeeded / sizeof( HMODULE ); i < count; ++i )
    {
        hIter = vars.hModules[i];
        if( hCaller )
        {
            /* Next modules can be used for RTLD_NEXT, this loop cannot be
             * reached without setting hCaller in this context anyways so
             * we just skip checking if handle == RTLD_NEXT */
            if( hCaller == hIter )
                hCaller = NULL;
            continue;
        }
        if( local_search( hIter ) )
            continue;
        symbol = GetProcAddress( hIter, name );
        if( symbol )
            break;
    }

    if ( i == count )
        dwMessageId = ERROR_PROC_NOT_FOUND;

freeHeap:
    dlsym_clear_heap( &vars );
end:
    if( dwMessageId )
        save_err_str( name, dwMessageId );

    return *(void **) (&symbol);
}

DLFCN_EXPORT
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

/* See https://docs.microsoft.com/en-us/archive/msdn-magazine/2002/march/inside-windows-an-in-depth-look-into-the-win32-portable-executable-file-format-part-2
 * for details */

/* Get specific image section */
static BOOL get_image_section( HMODULE module, int index, void **ptr, DWORD *size )
{
    IMAGE_DOS_HEADER *dosHeader;
    IMAGE_NT_HEADERS *ntHeaders;
    IMAGE_OPTIONAL_HEADER *optionalHeader;

    dosHeader = (IMAGE_DOS_HEADER *) module;

    if( dosHeader->e_magic != IMAGE_DOS_SIGNATURE )
        return FALSE;

    ntHeaders = (IMAGE_NT_HEADERS *) ( (BYTE *) dosHeader + dosHeader->e_lfanew );

    if( ntHeaders->Signature != IMAGE_NT_SIGNATURE )
        return FALSE;

    optionalHeader = &ntHeaders->OptionalHeader;

    if( optionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
        return FALSE;

    if( index < 0 || index >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES || index >= optionalHeader->NumberOfRvaAndSizes )
        return FALSE;

    if( optionalHeader->DataDirectory[index].Size == 0 || optionalHeader->DataDirectory[index].VirtualAddress == 0 )
        return FALSE;

    if( size != NULL )
        *size = optionalHeader->DataDirectory[index].Size;

    *ptr = (void *)( (BYTE *) module + optionalHeader->DataDirectory[index].VirtualAddress );

    return TRUE;
}

/* Return symbol name for a given address from export table */
static const char *get_export_symbol_name( HMODULE module, IMAGE_EXPORT_DIRECTORY *ied, const void *addr, void **func_address )
{
    DWORD i;
    void *candidateAddr = NULL;
    int candidateIndex = -1;
    BYTE *base = (BYTE *) module;
    DWORD *functionAddressesOffsets = (DWORD *) (base + (DWORD) ied->AddressOfFunctions);
    DWORD *functionNamesOffsets = (DWORD *) (base + (DWORD) ied->AddressOfNames);
    USHORT *functionNameOrdinalsIndexes = (USHORT *) (base + (DWORD) ied->AddressOfNameOrdinals);

    for( i = 0; i < ied->NumberOfFunctions; i++ )
    {
        if( (void *) ( base + functionAddressesOffsets[i] ) > addr || candidateAddr >= (void *) ( base + functionAddressesOffsets[i] ) )
            continue;

        candidateAddr = (void *) ( base + functionAddressesOffsets[i] );
        candidateIndex = i;
    }

    if( candidateIndex == -1 )
        return NULL;

    *func_address = candidateAddr;

    for( i = 0; i < ied->NumberOfNames; i++ )
    {
        if( functionNameOrdinalsIndexes[i] == candidateIndex )
            return (const char *) ( base + functionNamesOffsets[i] );
    }

    return NULL;
}

static BOOL is_valid_address( const void *addr )
{
    MEMORY_BASIC_INFORMATION info;
    size_t result;

    if( addr == NULL )
        return FALSE;

    /* check valid pointer */
    result = VirtualQuery( addr, &info, sizeof( info ) );

    if( !result || !(info.AllocationBase) || !(info.AllocationProtect) || info.AllocationProtect == PAGE_NOACCESS )
        return FALSE;

    return TRUE;
}

#if defined(_M_ARM64) || defined(__aarch64__)
static INT64 sign_extend(UINT64 value, UINT bits)
{
    const UINT left = 64 - bits;
    const INT64 m1 = -1;
    const INT64 wide = (INT64) (value << left);
    const INT64 sign = ( wide < 0 ) ? ( m1 << left ) : 0;

    return value | sign;
}
#endif

/* Return state if address points to an import thunk
 *
 * On x86, an import thunk is setup with a 'jmp' instruction followed by an
 * absolute address (32bit) or relative offset (64bit) pointing into
 * the import address table (iat), which is partially maintained by
 * the runtime linker.
 *
 * On ARM64, an import thunk is also a relative jump pointing into the
 * import address table, implemented by the following three instructions:
 *
 *      adrp x16, [page_offset]
 * Calculates the page address (aligned to 4KB) the IAT is at, based
 * on the value of x16, with page_offset.
 *
 *      ldr  x16, [x16, offset]
 * Calculates the final IAT address, x16 <- x16 + offset.
 *
 *      br   x16
 * Jump to the address in x16.
 *
 * The register used here is hardcoded to be x16.
 */
static BOOL is_import_thunk( const void *addr )
{
#if defined(_M_ARM64) || defined(__aarch64__)
    ULONG opCode1 = * (ULONG *) ( (BYTE *) addr );
    ULONG opCode2 = * (ULONG *) ( (BYTE *) addr + 4 );
    ULONG opCode3 = * (ULONG *) ( (BYTE *) addr + 8 );

    return (opCode1 & 0x9f00001f) == 0x90000010    /* adrp x16, [page_offset] */
        && (opCode2 & 0xffe003ff) == 0xf9400210    /* ldr  x16, [x16, offset] */
        && opCode3 == 0xd61f0200                   /* br   x16 */
        ? TRUE : FALSE;
#else
    return *(short *) addr == 0x25ff ? TRUE : FALSE;
#endif
}

/* Return adress from the import address table (iat),
 * if the original address points to a thunk table entry.
 */
static void *get_address_from_import_address_table( void *iat, DWORD iat_size, const void *addr )
{
    BYTE *thkp = (BYTE *) addr;
#if defined(_M_ARM64) || defined(__aarch64__)
    /*
     *  typical import thunk in ARM64:
     *  0x7ff772ae78c0 <+25760>: adrp   x16, 1
     *  0x7ff772ae78c4 <+25764>: ldr    x16, [x16, #0xdc0]
     *  0x7ff772ae78c8 <+25768>: br     x16
     */
    ULONG opCode1 = * (ULONG *) ( (BYTE *) addr );
    ULONG opCode2 = * (ULONG *) ( (BYTE *) addr + 4 );

    /* Extract the offset from adrp instruction */
    UINT64 pageLow2 = (opCode1 >> 29) & 3;
    UINT64 pageHigh19 = (opCode1 >> 5) & ~(~0ull << 19);
    INT64 page = sign_extend((pageHigh19 << 2) | pageLow2, 21) << 12;

    /* Extract the offset from ldr instruction */
    UINT64 offset = ((opCode2 >> 10) & ~(~0ull << 12)) << 3;

    /* Calculate the final address */
    BYTE *ptr = (BYTE *) ( (ULONG64) thkp & ~0xfffull ) + page + offset;
#else
    /* Get offset from thunk table (after instruction 0xff 0x25)
     *   4018c8 <_VirtualQuery>: ff 25 4a 8a 00 00
     */
    ULONG offset = *(ULONG *)( thkp + 2 );
#if defined(_M_AMD64) || defined(__x86_64__)
    /* On 64 bit the offset is relative
     *      4018c8:   ff 25 4a 8a 00 00    jmpq    *0x8a4a(%rip)    # 40a318 <__imp_VirtualQuery>
     * And can be also negative (MSVC in WDK)
     *   100002f20:   ff 25 3a e1 ff ff    jmpq   *-0x1ec6(%rip)    # 0x100001060
     * So cast to signed LONG type
     */
    BYTE *ptr = (BYTE *)( thkp + 6 + (LONG) offset );
#else
    /* On 32 bit the offset is absolute
     *   4019b4:    ff 25 90 71 40 00    jmp    *0x40719
     */
    BYTE *ptr = (BYTE *) offset;
#endif
#endif

    if( !is_valid_address( ptr ) || ptr < (BYTE *) iat || ptr > (BYTE *) iat + iat_size )
        return NULL;

    return *(void **) ptr;
}

/* Holds module filename */
static char module_filename[2*MAX_PATH];

static BOOL fill_info( const void *addr, Dl_info *info )
{
    HMODULE hModule;
    DWORD dwSize;
    IMAGE_EXPORT_DIRECTORY *ied;
    void *funcAddress = NULL;

    /* Get module of the specified address */
    hModule = MyGetModuleHandleFromAddress( addr );

    if( hModule == NULL )
        return FALSE;

    dwSize = GetModuleFileNameA( hModule, module_filename, sizeof( module_filename ) );

    if( dwSize == 0 || dwSize == sizeof( module_filename ) )
        return FALSE;

    info->dli_fname = module_filename;
    info->dli_fbase = (void *) hModule;

    /* Find function name and function address in module's export table */
    if( get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_EXPORT, (void **) &ied, NULL ) )
        info->dli_sname = get_export_symbol_name( hModule, ied, addr, &funcAddress );
    else
        info->dli_sname = NULL;

    info->dli_saddr = info->dli_sname == NULL ? NULL : funcAddress != NULL ? funcAddress : (void *) addr;

    return TRUE;
}

DLFCN_EXPORT
int dladdr( const void *addr, Dl_info *info )
{
    if( info == NULL )
        return 0;

    if( !is_valid_address( addr ) )
        return 0;

    if( is_import_thunk( addr ) )
    {
        void *iat;
        DWORD iatSize;
        HMODULE hModule;

        /* Get module of the import thunk address */
        hModule = MyGetModuleHandleFromAddress( addr );

        if( hModule == NULL )
            return 0;

        if( !get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_IAT, &iat, &iatSize ) )
        {
            /* Fallback for cases where the iat is not defined,
             * for example i586-mingw32msvc-gcc */
            IMAGE_IMPORT_DESCRIPTOR *iid;
            DWORD iidSize;

            if( !get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_IMPORT, (void **) &iid, &iidSize ) )
                return 0;

            if( iid == NULL || iid->Characteristics == 0 || iid->FirstThunk == 0 )
                return 0;

            iat = (void *)( (BYTE *) hModule + (DWORD) iid->FirstThunk );
            /* We assume that in this case iid and iat's are in linear order */
            iatSize = iidSize - (DWORD) ( (BYTE *) iat - (BYTE *) iid );
        }

        addr = get_address_from_import_address_table( iat, iatSize, addr );

        if( !is_valid_address( addr ) )
            return 0;
    }

    if( !fill_info( addr, info ) )
        return 0;

    return 1;
}

/* Keep initialiser/terminator code out of DllMain so it can be used in static
 * builds too */
static BOOL libinitcalled = FALSE;
static HMODULE hPsapi = NULL;
static void libterm( void )
{
    MyEnumProcessModules = Th32EnumProcessModules;
    if ( hPsapi )
    {
        FreeLibrary( hPsapi );
        hPsapi = NULL;
    }
    libinitcalled = FALSE;
}
static void libinit( void )
{
    HMODULE kernel32 = NULL;
    UINT uMode = 0;

    if ( libinitcalled )
        return;

    /* Do not let Windows display the critical-error-handler message box */
    uMode = MySetErrorMode( SEM_FAILCRITICALERRORS );

    kernel32 = GetModuleHandleA( "Kernel32.dll" );
    if( kernel32 )
    {
        SetThreadErrorModePtr = (SetThreadErrorModePtrCB)GetProcAddress( kernel32, "SetThreadErrorMode" );
        GetModuleHandleExAPtr = (GetModuleHandleExAPtrCB)GetProcAddress( kernel32, "GetModuleHandleExA" );
        MyGetProcessId = (GetProcessIdCB)GetProcAddress( kernel32, "K32GetProcessId");
        MyCreateToolhelp32Snapshot = (CreateToolhelp32SnapshotCB)GetProcAddress( kernel32, "K32CreateToolHelp32SnapShot" );
        MyModule32First = (Module32NextCB)GetProcAddress(kernel32, "K32Module32First" );
        MyModule32Next = (Module32NextCB)GetProcAddress(kernel32, "K32Module32Next" );

        if ( !SetThreadErrorModePtr )
            SetThreadErrorModePtr = MySetThreadErrorMode;

        if ( !GetModuleHandleExAPtr )
            GetModuleHandleExAPtr = HackyGetModuleHandleExA;

        if ( !MyGetProcessId )
            MyGetProcessId = (GetProcessIdCB)GetProcAddress( kernel32, "GetProcessId");

        if ( !MyCreateToolhelp32Snapshot )
            MyCreateToolhelp32Snapshot = (CreateToolhelp32SnapshotCB)GetProcAddress( kernel32, "CreateToolHelp32SnapShot" );

        if ( !MyModule32First )
            MyModule32First = (Module32NextCB)GetProcAddress(kernel32, "Module32First" );

        if ( !MyModule32Next )
            MyModule32Next = (Module32NextCB)GetProcAddress(kernel32, "Module32Next" );

        if ( !MyGetProcessId )
            MyGetProcessId = FailGetProcessId;

        if ( !MyCreateToolhelp32Snapshot )
            MyCreateToolhelp32Snapshot = FailCreateToolhelp32Snapshot;

        if ( !MyModule32First )
            MyModule32First = FailModule32Next;

        if ( !MyModule32Next )
            MyModule32Next = FailModule32Next;

        /* Windows 7 and newer versions have K32EnumProcessModules in Kernel32.dll which is always pre-loaded */
        MyEnumProcessModules  = (EnumProcessModulesPtrCB)GetProcAddress( kernel32, "K32EnumProcessModules" );
    }

    /* Windows Vista and older version have EnumProcessModules in Psapi.dll which needs to be loaded */
    if( !kernel32 || !MyEnumProcessModules )
    {
		if ( !hPsapi )
			hPsapi = LoadLibraryA( "Psapi.dll" );
        if ( !hPsapi )
            MyEnumProcessModules = Th32EnumProcessModules;
        else
        {
            MyEnumProcessModules = (EnumProcessModulesPtrCB)GetProcAddress( hPsapi, "EnumProcessModules" );
            if( !MyEnumProcessModules )
                libterm();
        }
    }

    MySetErrorMode( uMode );
    libinitcalled = TRUE;
}

#ifdef __GNUC__
static void _libinit(void) { libinit(); } __attribute__((constructor));
static void _libterm(void) { libterm(); } __attribute__((deconstructor));
#else
__declspec(allocate(".CRT$XCU")) void _libinit( void )
{
    libinit();
    /* Not a fan of this since this involves a possible memory allocation */
    atexit(libterm);
}
#endif

#ifdef DLFCN_WIN32_SHARED
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvTerminated )
{
    (void) hinstDLL;
    (void) lpvTerminated;
    libinit();

    /* Just a fallback for MSVC, not sure the atexit() will work out ther after
     * all */
    if ( fdwReason == DLL_PROCESS_DETACH )
		libterm();
    return TRUE;
}
#endif
