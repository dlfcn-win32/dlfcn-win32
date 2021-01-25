/* On Unix like os compile with "-Wl,--export-dynamic -fpie" (default with cmake) */
/* On Windows gcc compile with "-Wl,--export-all-symbols" (default with cmake) */
/* On Windows msvc compile with "/EXPORT:dlopen /EXPORT:dladdr" (default with cmake) */

/* required for non Windows builds, must be set in front of the first system include */
#define _GNU_SOURCE

#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verbose = 0;

typedef enum {
    Pass = 1,
    PassWithoutSymbol = 2,
    PassWithDifferentAddress = 3,
    Fail = 0,
    NoInfo = -1,
} ExpectedResult;

typedef void (* func) (void);

static void print_dl_info( Dl_info *info, char *prefix, char *suffix )
{
    printf( "%sfilename: %s base: %p symbol name: '%s' addr: %p%s", prefix, info->dli_fname, info->dli_fbase, info->dli_sname, info->dli_saddr, suffix );
}

/**
 * @brief check information returned by dladdr
 * @param hint text describing what to test
 * @param addr address to check
 * @param addrsym
 * @param expected check against expected result
 * @return 0 check passed
 * @return 1 check failed
 */
static int check_dladdr( const char *hint, void *addr, char *addrsym, ExpectedResult expected_result )
{
    Dl_info info;
    int result = dladdr( addr, &info );
    int passed = 0;
    if (!result)
    {
        passed = expected_result == NoInfo || expected_result == Fail;
    }
    else
    {
        int sym_match  = info.dli_sname && strcmp( addrsym, info.dli_sname ) == 0;
        int addr_match  = addr == info.dli_saddr;
        passed = (expected_result == Pass && sym_match && addr_match)
                 || (expected_result == PassWithoutSymbol && addr_match && !info.dli_sname)
                 || (expected_result == PassWithDifferentAddress && sym_match && !addr_match)
                 || (expected_result == Fail && (!sym_match || !addr_match));
    }
    printf( "checking '%s' - address %p which should have symbol '%s' -> %s%s", hint, addr, addrsym, passed ? "passed" : "failed", verbose || !passed ? " " : "\n" );
    if( verbose || !passed )
    {
        if( !result )
            printf( "(could not get symbol information for address %p)\n", addr );
        else
            print_dl_info( &info, "(", ")\n");
    }
    return !passed;
}

#ifdef _WIN32
/**
 * @brief check address from a symbol located in a shared lilbrary
 * @param hint text describing what to test
 * @param libname libray to get the address from
 * @param addrsym symbol to get the address for
 * @param should_match result should match the given values
 * @return 0 check passed
 * @return 1 check failed
 * @return 2 failed to open library
 * @return 3 failed to get symbol address
 */
static int check_dladdr_by_dlopen( const char *hint, char *libname, char *sym, int should_match )
{
    void *library = NULL;
    void *addr = NULL;
    int result;

    library = dlopen( libname, RTLD_GLOBAL );
    if ( library == NULL )
    {
        fprintf( stderr, "could not open '%s'\n", libname );
        return 2;
    }

    addr = dlsym( library, sym );
    if ( !addr ) {
        fprintf( stderr, "could not get address from library '%s' for symbol '%s'\n", libname, sym );
        return 3;
    }

    result = check_dladdr( hint, addr, sym, should_match );
    dlclose( library );

    return result;
}
#endif

#ifdef _WIN32
#include <windows.h>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
/* hide warning "reclared without 'dllimport' attribute" */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#elif defined(_MSC_VER)
/* disable warning C4273 inconsistent dll linkage */
#pragma warning(push)
#pragma warning(disable: 4273)
#endif
/*
 * Windows API functions decorated with WINBASEAPI are imported from iat.
 * For testing force linking by import thunk for the following functions
 */
HMODULE WINAPI GetModuleHandleA (LPCSTR lpModuleName);
SIZE_T WINAPI VirtualQuery (LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
/* force linking by iat */
/* WINBASEAPI, which is normally used here, can be overriden by compiler or application, so we cannot use it */
__declspec(dllimport) HMODULE WINAPI LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD  dwFlags);
__declspec(dllimport) int __cdecl atoi(const char *_Str);

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif

#define UNUSED(x) (void)x

#ifdef _WIN32
__declspec(dllexport)
#endif
int main(int argc, char **argv)
{
    /* points to non reachable address */
    unsigned char zero_thunk_address[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    /* points to executable base */
    unsigned char invalid_thunk_address[6] = { 0xFF, 0x25, 0x00, 0x00, 0x40, 0x00 };
    /* no import thunk */
    unsigned char no_import_thunk[6] = { 0xFF, 0x26, 0x00, 0x00, 0x40, 0x00 };
    int  result = 0;
    UNUSED(argv);

    if (argc == 2)
        verbose = 1;

    result = check_dladdr( "null pointer", (void*)0, NULL , NoInfo);
    result |= check_dladdr( "invalid pointer", (void*)0x125, NULL , NoInfo);
    result |= check_dladdr( "function from dl library", (void*)dladdr, "dladdr" , Pass );
    result |= check_dladdr( "function from dl library", (void*)dlopen, "dlopen", Pass );
    result |= check_dladdr( "function from glibc/msvcrt library", (void*)atoi, "atoi", Pass );
    result |= check_dladdr( "function from executable", (void*)main, "main", Pass );
    result |= check_dladdr( "static function from executable", (void*)print_dl_info, "print_dl_info", Fail );
    result |= check_dladdr( "address with positive offset", ((char*)atoi)+1, "atoi", PassWithDifferentAddress );
    result |= check_dladdr( "zero address from import thunk", zero_thunk_address, "", NoInfo );
    result |= check_dladdr( "invalid address from import thunk", invalid_thunk_address, "", NoInfo );
    result |= check_dladdr( "no import thunk", no_import_thunk, "", NoInfo );

#ifdef _WIN32
    result |= check_dladdr( "last entry in iat", (void*)VirtualQuery, "VirtualQuery", PassWithDifferentAddress );

    result |= check_dladdr ( "address through import thunk", (void*)GetModuleHandleA, "GetModuleHandleA", PassWithDifferentAddress );
    result |= check_dladdr_by_dlopen( "address by dlsym", "kernel32.dll", "GetModuleHandleA", Pass );

    result |= check_dladdr ( "address by image allocation table", (void*)LoadLibraryExA, "LoadLibraryExA", Pass );
    result |= check_dladdr_by_dlopen( "address by dlsym", "kernel32.dll", "LoadLibraryExA", Pass );
#endif
    return result;
}
