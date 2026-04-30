/*
 * mu_mmgr.h
 *
 * Standalone C99 single-header conversion of Fluid Studios MemoryManager
 * (from mmgr.h + mmgr.c as integrated in The Forge).
 *
 * Usage:
 *   #define MU_MMGR_IMPLEMENTATION
 *   #include "mu_mmgr.h"
 *
 * Typical flow:
 *   initMemAlloc("my_app");
 *   void* p = MMGR_MALLOC(256);
 *   p = MMGR_REALLOC(p, 512);
 *   MMGR_FREE(p);
 *   exitMemAlloc(); // emits leak report and asserts on leaks by default
 */

#ifndef MU_MMGR_H
#define MU_MMGR_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(_WIN32) && !defined(XBOX)
#define __FUNCTION__ __func__
#endif

/* Linux/glibc gets execinfo by default. */
#ifndef MMGR_BACKTRACE
#if defined(__linux__) && defined(__GLIBC__)
#define MMGR_BACKTRACE 1
#else
#define MMGR_BACKTRACE 0
#endif
#endif

#ifndef MMGR_BACKTRACE_SIZE
#define MMGR_BACKTRACE_SIZE 128
#endif

#ifndef MMGR_ASSERT
#define MMGR_ASSERT(expr) assert(expr)
#endif

#ifndef MMGR_ASSERT_ON_LEAK
#define MMGR_ASSERT_ON_LEAK 1
#endif

#ifndef MMGR_HASH_BITS
#define MMGR_HASH_BITS 12u
#endif

/* Optional: mirror legacy controls from original source. */
/* #define STRESS_TEST */
/* #define TEST_MEMORY_MANAGER */
/* #define RANDOM_FAILURE 10.0 */
/* #define FORCE_EXACT_ALIGNMENT */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tag_au
{
    size_t actualSize;
    size_t reportedSize;
    size_t alignment;
    size_t offset;
    void*  actualAddress;
    void*  reportedAddress;
    char   sourceFile[140];
    char   sourceFunc[140];
#if MMGR_BACKTRACE
    void* backtrace_buffer[MMGR_BACKTRACE_SIZE];
    int   backtrace_nptrs;
    int   backtrace_skip;
#endif
    unsigned int   sourceLine;
    unsigned int   allocationType;
    bool           breakOnDealloc;
    bool           breakOnRealloc;
    unsigned int   allocationNumber;
    struct tag_au* next;
    struct tag_au* prev;
} sAllocUnit;

typedef struct
{
    unsigned int totalReportedMemory;
    unsigned int totalActualMemory;
    unsigned int peakReportedMemory;
    unsigned int peakActualMemory;
    unsigned int accumulatedReportedMemory;
    unsigned int accumulatedActualMemory;
    unsigned int accumulatedAllocUnitCount;
    unsigned int totalAllocUnitCount;
    unsigned int peakAllocUnitCount;
} sMStats;

typedef sMStats MemoryStatistics;

enum
{
    m_alloc_unknown,
    m_alloc_new,
    m_alloc_new_array,
    m_alloc_malloc,
    m_alloc_calloc,
    m_alloc_realloc,
    m_alloc_delete,
    m_alloc_delete_array,
    m_alloc_free,
};

/* Legacy API from mmgr.h */
void  mmgrSetOwner(const char* file, const unsigned int line, const char* func);
bool* mmgrBreakOnRealloc(void* reportedAddress);
bool* mmgrBreakOnDealloc(void* reportedAddress);
void* mmgrAllocator(const char*        sourceFile,
                    const unsigned int sourceLine,
                    const char*        sourceFunc,
                    const unsigned int allocationType,
                    size_t             alignment,
                    size_t             reportedSize);
void* mmgrReallocator(const char*        sourceFile,
                      const unsigned int sourceLine,
                      const char*        sourceFunc,
                      const unsigned int reallocationType,
                      size_t             reportedSize,
                      void*              reportedAddress);
void  mmgrDeallocator(const char*        sourceFile,
                      const unsigned int sourceLine,
                      const char*        sourceFunc,
                      const unsigned int deallocationType,
                      const void*        reportedAddress);

bool mmgrValidateAddress(const void* reportedAddress);
bool mmgrValidateAllocUnit(const sAllocUnit* allocUnit);
bool mmgrValidateAllAllocUnits(void);

unsigned int mmgrCalcUnused(const sAllocUnit* allocUnit);
unsigned int mmgrCalcAllUnused(void);

void    mmgrSetExecutableName(const char* name, size_t length);
void    mmgrSetLogFileDirectory(const char* directory);
void    mmgrDumpAllocUnit(const sAllocUnit* allocUnit, const char* prefix);
void    mmgrDumpMemoryReport(const char* filename, const bool overwrite);
sMStats mmgrGetMemoryStatistics(void);

/* Additional public helpers present in the original mmgr.c integration. */
bool             initMemAlloc(const char* appName);
void             exitMemAlloc(void);
MemoryStatistics memGetStatistics(void);

bool* m_alwaysValidateAll(void);
bool* m_alwaysLogAll(void);
bool* m_alwaysWipeAll(void);
bool* m_randomeWipe(void);
void  m_breakOnAllocation(unsigned int count);
void  memSetStackSkipCount(int stackDepth);

/* Convenience macros for C-only call sites. */
#define MMGR_MALLOC(sz) mmgrAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sizeof(void*), (sz))
#define MMGR_MEMALIGN(al, sz) mmgrAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, (al), (sz))
#define MMGR_CALLOC(cnt, sz)                                                                                           \
    mmgrAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_calloc, sizeof(void*), (cnt) * (sz))
#define MMGR_REALLOC(ptr, sz) mmgrReallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_realloc, (sz), (ptr))
#define MMGR_FREE(ptr) mmgrDeallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_free, (ptr))

#ifdef __cplusplus
}
#endif

#endif /* MU_MMGR_H */

#ifdef MU_MMGR_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if MMGR_BACKTRACE
#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#else
#include <execinfo.h>
#endif
#endif

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION mu_mmgr_mutex_t;
static void              mu_mmgr_mutex_init(mu_mmgr_mutex_t* m)
{
    InitializeCriticalSection(m);
}
static void mu_mmgr_mutex_lock(mu_mmgr_mutex_t* m)
{
    EnterCriticalSection(m);
}
static void mu_mmgr_mutex_unlock(mu_mmgr_mutex_t* m)
{
    LeaveCriticalSection(m);
}
static int mu_mmgr_localtime_safe(const time_t* t, struct tm* out)
{
    return localtime_s(out, t);
}
#else
#include <pthread.h>
typedef pthread_mutex_t mu_mmgr_mutex_t;
static void             mu_mmgr_mutex_init(mu_mmgr_mutex_t* m)
{
    (void)pthread_mutex_init(m, NULL);
}
static void mu_mmgr_mutex_lock(mu_mmgr_mutex_t* m)
{
    (void)pthread_mutex_lock(m);
}
static void mu_mmgr_mutex_unlock(mu_mmgr_mutex_t* m)
{
    (void)pthread_mutex_unlock(m);
}
static int mu_mmgr_localtime_safe(const time_t* t, struct tm* out)
{
    return localtime_r(t, out) == NULL ? -1 : 0;
}
#endif

#ifndef MU_MMGR_LOG_TO_STDERR
#define MU_MMGR_LOG_TO_STDERR 0
#endif

#ifndef MU_MMGR_PATH_MAX
#define MU_MMGR_PATH_MAX 1024
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MU_MMGR_UNUSED(x) (void)(x)
#else
#define MU_MMGR_UNUSED(x) ((void)(x))
#endif

#ifdef STRESS_TEST
static bool               mu_mmgr_randomWipe           = true;
static bool               mu_mmgr_alwaysValidateAll    = true;
static bool               mu_mmgr_alwaysLogAll         = true;
static bool               mu_mmgr_alwaysWipeAll        = true;
static bool               mu_mmgr_cleanupLogOnFirstRun = true;
static const unsigned int mu_mmgr_paddingSize          = 1024;
#else
static bool               mu_mmgr_randomWipe           = false;
static bool               mu_mmgr_alwaysValidateAll    = false;
static bool               mu_mmgr_alwaysLogAll         = false;
static bool               mu_mmgr_alwaysWipeAll        = true;
static bool               mu_mmgr_cleanupLogOnFirstRun = true;
static const unsigned int mu_mmgr_paddingSize          = 4;
#endif

static unsigned int mu_mmgr_prefixPattern   = 0xbaadf00d;
static unsigned int mu_mmgr_postfixPattern  = 0xdeadc0de;
static unsigned int mu_mmgr_unusedPattern   = 0xfeedface;
static unsigned int mu_mmgr_releasedPattern = 0xdeadbeef;

enum
{
    mu_mmgr_hashSize = (1u << MMGR_HASH_BITS)
};

static const char* mu_mmgr_allocationTypes[] = {"Unknown", "new",    "new[]",    "malloc", "calloc",
                                                "realloc", "delete", "delete[]", "free"};

static sAllocUnit*  mu_mmgr_hashTable[mu_mmgr_hashSize];
static sAllocUnit*  mu_mmgr_reservoir              = NULL;
static unsigned int mu_mmgr_currentAllocationCount = 0;
static unsigned int mu_mmgr_breakOnAllocationCount = 0;
static sMStats      mu_mmgr_stats;
static const char*  mu_mmgr_sourceFile                     = "??";
static const char*  mu_mmgr_sourceFunc                     = "??";
static unsigned int mu_mmgr_sourceLine                     = 0;
static sAllocUnit** mu_mmgr_reservoirBuffer                = NULL;
static unsigned int mu_mmgr_reservoirBufferSize            = 0;
static char*        mu_mmgr_logMemory                      = NULL;
static size_t       mu_mmgr_logMemoryLength                = 0;
static const char*  mu_mmgr_appName                        = NULL;
static char         mu_mmgr_executableName[256]            = {0};
static char         mu_mmgr_logDirectory[MU_MMGR_PATH_MAX] = {0};

static mu_mmgr_mutex_t mu_mmgr_allocMutex;
static mu_mmgr_mutex_t mu_mmgr_logMutex;
static bool            mu_mmgr_allocMutexInit = false;
static bool            mu_mmgr_logMutexInit   = false;

#if MMGR_BACKTRACE
static int mu_mmgr_stackSkipCount = 0;
#if defined(_WIN32)
static HANDLE mu_mmgr_processHandle = NULL;
#endif
#endif

#define MU_MMGR_BUFFER_SIZE 2048
#define MU_MMGR_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void mu_mmgr_lock_alloc(void)
{
    if(!mu_mmgr_allocMutexInit)
    {
        mu_mmgr_mutex_init(&mu_mmgr_allocMutex);
        mu_mmgr_allocMutexInit = true;
    }
    mu_mmgr_mutex_lock(&mu_mmgr_allocMutex);
}

static void mu_mmgr_unlock_alloc(void)
{
    if(mu_mmgr_allocMutexInit)
    {
        mu_mmgr_mutex_unlock(&mu_mmgr_allocMutex);
    }
}

static void mu_mmgr_lock_log(void)
{
    if(!mu_mmgr_logMutexInit)
    {
        mu_mmgr_mutex_init(&mu_mmgr_logMutex);
        mu_mmgr_logMutexInit = true;
    }
    mu_mmgr_mutex_lock(&mu_mmgr_logMutex);
}

static void mu_mmgr_unlock_log(void)
{
    if(mu_mmgr_logMutexInit)
    {
        mu_mmgr_mutex_unlock(&mu_mmgr_logMutex);
    }
}

static char* mu_mmgr_strncpy(char* dst, size_t dstSize, const char* src, size_t count)
{
    size_t size = (dstSize < count) ? (dstSize - 1) : count;
    char*  ret  = strncpy(dst, src, size);
    ret[size]   = '\0';
    return ret;
}

static char* mu_mmgr_strcpy(char* dst, size_t dstSize, const char* src)
{
    size_t srcSize = strlen(src);
    return mu_mmgr_strncpy(dst, dstSize, src, srcSize);
}

static const char* mu_mmgr_sourceFileStripper(const char* sourceFile)
{
    const char* ptr = strrchr(sourceFile, '\\');
    if(ptr)
    {
        return ptr + 1;
    }
    ptr = strrchr(sourceFile, '/');
    if(ptr)
    {
        return ptr + 1;
    }
    return sourceFile;
}

static const char* mu_mmgr_ownerString(const char* sourceFile, const unsigned int sourceLine, const char* sourceFunc)
{
    static char str[180];
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%s(%05u)::%s", mu_mmgr_sourceFileStripper(sourceFile), sourceLine, sourceFunc);
    return str;
}

static const char* mu_mmgr_insertCommas(unsigned int value)
{
    static char str[30];
    memset(str, 0, sizeof(str));

    snprintf(str, sizeof(str), "%u", value);
    if(strlen(str) > 3)
    {
        memmove(&str[strlen(str) - 3], &str[strlen(str) - 4], 4);
        str[strlen(str) - 4] = ',';
    }
    if(strlen(str) > 7)
    {
        memmove(&str[strlen(str) - 7], &str[strlen(str) - 8], 8);
        str[strlen(str) - 8] = ',';
    }
    if(strlen(str) > 11)
    {
        memmove(&str[strlen(str) - 11], &str[strlen(str) - 12], 12);
        str[strlen(str) - 12] = ',';
    }

    return str;
}

static const char* mu_mmgr_memorySizeString(uint32_t size)
{
    static char str[90];
    if(size > (1024u * 1024u))
    {
        snprintf(str, sizeof(str), "%10s (%7.2fM)", mu_mmgr_insertCommas(size), ((float)size) / (1024.0f * 1024.0f));
    }
    else if(size > 1024u)
    {
        snprintf(str, sizeof(str), "%10s (%7.2fK)", mu_mmgr_insertCommas(size), ((float)size) / 1024.0f);
    }
    else
    {
        snprintf(str, sizeof(str), "%10s bytes     ", mu_mmgr_insertCommas(size));
    }
    return str;
}

static size_t mu_mmgr_calculateActualSize(const size_t reportedSize)
{
    return reportedSize + mu_mmgr_paddingSize * sizeof(uint32_t) * 2;
}

static void* mu_mmgr_calculateReportedAddress(const void* actualAddress)
{
    if(!actualAddress)
    {
        return NULL;
    }
    return (void*)(((const char*)actualAddress) + sizeof(uint32_t) * mu_mmgr_paddingSize);
}

static void mu_mmgr_resetGlobals(void)
{
    mu_mmgr_sourceFile = "??";
    mu_mmgr_sourceLine = 0;
    mu_mmgr_sourceFunc = "??";
#if MMGR_BACKTRACE
    mu_mmgr_stackSkipCount = 0;
#endif
}

static sAllocUnit* mu_mmgr_findAllocUnit(const void* reportedAddress)
{
    MMGR_ASSERT(reportedAddress != NULL);

    size_t      hashIndex = (((size_t)reportedAddress) >> 4u) & (mu_mmgr_hashSize - 1u);
    sAllocUnit* ptr       = mu_mmgr_hashTable[hashIndex];
    while(ptr)
    {
        if(ptr->reportedAddress == reportedAddress)
        {
            return ptr;
        }
        ptr = ptr->next;
    }

    return NULL;
}

static char* mu_mmgr_logToMemory(const char* log)
{
    const size_t logLength = strlen(log) + 1;

    if(mu_mmgr_logMemory == NULL)
    {
        mu_mmgr_logMemory       = (char*)calloc(1, sizeof(char));
        mu_mmgr_logMemoryLength = 1;
    }

    if(!mu_mmgr_logMemory)
    {
        return NULL;
    }

    char* newMem = (char*)realloc(mu_mmgr_logMemory, mu_mmgr_logMemoryLength + logLength - 1);
    if(!newMem)
    {
        return mu_mmgr_logMemory;
    }

    mu_mmgr_logMemory = newMem;
    memcpy(mu_mmgr_logMemory + mu_mmgr_logMemoryLength - 1, log, logLength);
    mu_mmgr_logMemoryLength += logLength - 1;
    return mu_mmgr_logMemory;
}

static void mu_mmgr_doCleanupLogOnFirstRun(void)
{
    if(!mu_mmgr_cleanupLogOnFirstRun)
    {
        return;
    }

    mu_mmgr_cleanupLogOnFirstRun = false;

    (void)mu_mmgr_logToMemory("--------------------------------------------------------------------------------\n");
    (void)mu_mmgr_logToMemory("\n");
    (void)mu_mmgr_logToMemory("This file contains a log of all memory operations performed during the last run.\n");
    (void)mu_mmgr_logToMemory("\n");
    (void)mu_mmgr_logToMemory("Search for [!] to find allocator errors.\n");
    (void)mu_mmgr_logToMemory("--------------------------------------------------------------------------------\n");
}

static char* mu_mmgr_log(const char* format, ...)
{
    if(mu_mmgr_cleanupLogOnFirstRun)
    {
        mu_mmgr_doCleanupLogOnFirstRun();
    }

    mu_mmgr_lock_log();

    static char buffer[MU_MMGR_BUFFER_SIZE];
    va_list     ap;
    va_start(ap, format);
    int charsWritten = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if(charsWritten < 0)
    {
        charsWritten = 0;
    }

    int newlinePos         = charsWritten;
    int lastNewlinePos     = (int)sizeof(buffer) - 2;
    newlinePos             = newlinePos > lastNewlinePos ? lastNewlinePos : newlinePos;
    buffer[newlinePos]     = '\n';
    buffer[newlinePos + 1] = '\0';

#if MU_MMGR_LOG_TO_STDERR
    fputs(buffer, stderr);
#endif

    char* logAddress = mu_mmgr_logToMemory(buffer);

    mu_mmgr_unlock_log();
    return logAddress;
}

static void mu_mmgr_wipeWithPattern(sAllocUnit* allocUnit, uint32_t pattern, const unsigned int originalReportedSize)
{
    if(mu_mmgr_randomWipe)
    {
        pattern = ((uint32_t)(rand() & 0xff) << 24) | ((uint32_t)(rand() & 0xff) << 16)
                  | ((uint32_t)(rand() & 0xff) << 8) | (uint32_t)(rand() & 0xff);
    }

    if(mu_mmgr_alwaysWipeAll && allocUnit->reportedSize > originalReportedSize)
    {
        uint32_t* lptr   = (uint32_t*)(((char*)allocUnit->reportedAddress) + originalReportedSize);
        int       length = (int)(allocUnit->reportedSize - originalReportedSize);
        int       i;
        for(i = 0; i < (length >> 2); i++, lptr++)
        {
            *lptr = pattern;
        }

        unsigned int shiftCount = 0;
        char*        cptr       = (char*)(lptr);
        for(i = 0; i < (length & 0x3); i++, cptr++, shiftCount += 8)
        {
            *cptr = (char)((pattern & (0xffu << shiftCount)) >> shiftCount);
        }
    }

    uint8_t* pre  = (uint8_t*)allocUnit->reportedAddress - mu_mmgr_paddingSize * sizeof(uint32_t);
    uint8_t* post = (uint8_t*)allocUnit->reportedAddress + allocUnit->reportedSize;

    size_t paddingBytes = mu_mmgr_paddingSize * sizeof(uint32_t);
    size_t i;
    for(i = 0; i < paddingBytes; i++, pre++, post++)
    {
        *pre  = (uint8_t)((mu_mmgr_prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFFu);
        *post = (uint8_t)((mu_mmgr_postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFFu);
    }
}

static void mu_mmgr_dumpLine(FILE* fileToWrite, const char* format, ...)
{
    va_list args;
    char    buffer[MU_MMGR_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

#if MU_MMGR_LOG_TO_STDERR
    fprintf(stderr, "%s\n", buffer);
#endif

    if(fileToWrite)
    {
        fprintf(fileToWrite, "%s\n", buffer);
        fflush(fileToWrite);
    }
}

#if MMGR_BACKTRACE
static void mu_mmgr_dumpBacktrace(FILE* fh, const sAllocUnit* ptr)
{
    if(!ptr->backtrace_nptrs)
    {
        return;
    }

#if defined(_WIN32)
    MU_MMGR_UNUSED(fh);
    MU_MMGR_UNUSED(ptr);
#else
    char** strings = backtrace_symbols(ptr->backtrace_buffer, ptr->backtrace_nptrs);
    if(strings)
    {
        int j;
        for(j = ptr->backtrace_skip; j < ptr->backtrace_nptrs; ++j)
        {
            if(fh)
            {
                mu_mmgr_dumpLine(fh, "\t%s", strings[j]);
            }
            else
            {
                mu_mmgr_log("\t%s", strings[j]);
            }
        }
        free(strings);
    }
#endif
}
#endif

static void mu_mmgr_dumpAllocations(FILE* fh)
{
    mu_mmgr_dumpLine(fh, "Alloc.        Addr           Size           Addr           Size                        BreakOn BreakOn");
    mu_mmgr_dumpLine(fh, "Number      Reported       Reported        Actual         Actual     Unused    Method  Dealloc Realloc  Allocated by");
    mu_mmgr_dumpLine(fh, "------ ------------------ ---------- ------------------ ---------- ---------- -------- ------- ------- ---------------------------------------------------");

    for(unsigned int i = 0; i < mu_mmgr_hashSize; ++i)
    {
        sAllocUnit* ptr = mu_mmgr_hashTable[i];
        while(ptr)
        {
            mu_mmgr_dumpLine(fh, "% 6u 0x%016zX 0x%08zX 0x%016zX 0x%08zX 0x%08X %-8s    %c       %c    %s",
                             ptr->allocationNumber, (size_t)ptr->reportedAddress, ptr->reportedSize, (size_t)ptr->actualAddress,
                             ptr->actualSize, mmgrCalcUnused(ptr), mu_mmgr_allocationTypes[ptr->allocationType],
                             ptr->breakOnDealloc ? 'Y' : 'N', ptr->breakOnRealloc ? 'Y' : 'N',
                             mu_mmgr_ownerString(ptr->sourceFile, ptr->sourceLine, ptr->sourceFunc));
#if MMGR_BACKTRACE
            mu_mmgr_dumpBacktrace(fh, ptr);
#endif
            ptr = ptr->next;
        }
    }
}

static void mu_mmgr_buildPath(char* dst, size_t dstSize, const char* filename)
{
    if(mu_mmgr_logDirectory[0] != '\0')
    {
        size_t n = strlen(mu_mmgr_logDirectory);
        if(n > 0 && (mu_mmgr_logDirectory[n - 1] == '/' || mu_mmgr_logDirectory[n - 1] == '\\'))
        {
            snprintf(dst, dstSize, "%s%s", mu_mmgr_logDirectory, filename);
        }
        else
        {
            snprintf(dst, dstSize, "%s/%s", mu_mmgr_logDirectory, filename);
        }
    }
    else
    {
        snprintf(dst, dstSize, "%s", filename);
    }
}

static void mu_mmgr_dumpLeakReport(void)
{
    FILE* fh = NULL;
    char  outputFileName[MU_MMGR_PATH_MAX];
    memset(outputFileName, 0, sizeof(outputFileName));

    if(mu_mmgr_appName && mu_mmgr_appName[0] != '\0')
    {
        char localName[300];
        snprintf(localName, sizeof(localName), "%s.memleaks", mu_mmgr_appName);
        mu_mmgr_buildPath(outputFileName, sizeof(outputFileName), localName);
        fh = fopen(outputFileName, "wb");
        if(!fh)
        {
            snprintf(outputFileName, sizeof(outputFileName), "%s", "Failed to write report to file");
        }
    }

    time_t    t = time(NULL);
    struct tm tme;
    memset(&tme, 0, sizeof(tme));
    (void)mu_mmgr_localtime_safe(&t, &tme);

    mu_mmgr_dumpLine(fh, " -------------------------------------------------------------------------------");
    mu_mmgr_dumpLine(fh, "                Memory leak report %02d/%02d/%04d %02d:%02d:%02d:", tme.tm_mon + 1,
                     tme.tm_mday, tme.tm_year + 1900, tme.tm_hour, tme.tm_min, tme.tm_sec);
    mu_mmgr_dumpLine(fh, "                %s", outputFileName[0] ? outputFileName : "(no file)");
    mu_mmgr_dumpLine(fh, " -------------------------------------------------------------------------------");

    if(mu_mmgr_stats.totalAllocUnitCount)
    {
        mu_mmgr_dumpLine(fh, "%u memory leak%s found:\n", mu_mmgr_stats.totalAllocUnitCount,
                         mu_mmgr_stats.totalAllocUnitCount == 1 ? "" : "s");
        mu_mmgr_dumpAllocations(fh);
    }
    else
    {
        mu_mmgr_dumpLine(fh, "Congratulations! No memory leaks found!");

        if(mu_mmgr_reservoirBuffer)
        {
            for(unsigned int i = 0; i < mu_mmgr_reservoirBufferSize; ++i)
            {
                free(mu_mmgr_reservoirBuffer[i]);
            }
            free(mu_mmgr_reservoirBuffer);
            mu_mmgr_reservoirBuffer     = NULL;
            mu_mmgr_reservoirBufferSize = 0;
            mu_mmgr_reservoir           = NULL;
        }
    }

    {
        char* allMemoryLog = mu_mmgr_log("----All Allocations and Deallocations----");
        if(allMemoryLog)
        {
            mu_mmgr_dumpLine(fh, "%s", allMemoryLog);
        }
    }

    if(fh)
    {
        fclose(fh);
    }

#if MMGR_ASSERT_ON_LEAK
    MMGR_ASSERT(mu_mmgr_stats.totalAllocUnitCount == 0 && "Memory leaks found");
#endif
}

bool initMemAlloc(const char* appName)
{
    mu_mmgr_appName = appName;
    mu_mmgr_doCleanupLogOnFirstRun();

#if MMGR_BACKTRACE && defined(_WIN32)
    HANDLE currentProcess = GetCurrentProcess();
    DuplicateHandle(currentProcess, currentProcess, currentProcess, &mu_mmgr_processHandle, 0, TRUE, DUPLICATE_SAME_ACCESS);
#endif

    return true;
}

void exitMemAlloc(void)
{
    mu_mmgr_dumpLeakReport();

#if MMGR_BACKTRACE && defined(_WIN32)
    if(mu_mmgr_processHandle)
    {
        CloseHandle(mu_mmgr_processHandle);
        mu_mmgr_processHandle = NULL;
    }
#endif
}

MemoryStatistics memGetStatistics(void)
{
    MemoryStatistics result = {
        (uint32_t)mu_mmgr_stats.totalReportedMemory,       (uint32_t)mu_mmgr_stats.totalActualMemory,
        (uint32_t)mu_mmgr_stats.peakReportedMemory,        (uint32_t)mu_mmgr_stats.peakActualMemory,
        (uint32_t)mu_mmgr_stats.accumulatedReportedMemory, (uint32_t)mu_mmgr_stats.accumulatedActualMemory,
        (uint32_t)mu_mmgr_stats.accumulatedAllocUnitCount, (uint32_t)mu_mmgr_stats.totalAllocUnitCount,
        (uint32_t)mu_mmgr_stats.peakAllocUnitCount,
    };
    return result;
}

bool* m_alwaysValidateAll(void)
{
    return &mu_mmgr_alwaysValidateAll;
}
bool* m_alwaysLogAll(void)
{
    return &mu_mmgr_alwaysLogAll;
}
bool* m_alwaysWipeAll(void)
{
    return &mu_mmgr_alwaysWipeAll;
}
bool* m_randomeWipe(void)
{
    return &mu_mmgr_randomWipe;
}
void m_breakOnAllocation(unsigned int count)
{
    mu_mmgr_breakOnAllocationCount = count;
}

void mmgrSetOwner(const char* file, const unsigned int line, const char* func)
{
    if(mu_mmgr_sourceLine && mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[I] NOTE! Possible destructor chain: previous owner is %s",
                    mu_mmgr_ownerString(mu_mmgr_sourceFile, mu_mmgr_sourceLine, mu_mmgr_sourceFunc));
    }

    mu_mmgr_sourceFile = file ? file : "??";
    mu_mmgr_sourceLine = line;
    mu_mmgr_sourceFunc = func ? func : "??";
}

void memSetStackSkipCount(int stackDepth)
{
#if MMGR_BACKTRACE
    if(!mu_mmgr_stackSkipCount)
    {
        mu_mmgr_stackSkipCount = stackDepth;
    }
#else
    MU_MMGR_UNUSED(stackDepth);
#endif
}

void* mmgrAllocator(const char*        sourceFile,
                    const unsigned int sourceLine,
                    const char*        sourceFunc,
                    const unsigned int allocationType,
                    size_t             alignment,
                    size_t             reportedSize)
{
    if(mu_mmgr_cleanupLogOnFirstRun)
    {
        MMGR_ASSERT(false && "Memory tracker not initialized");
        return NULL;
    }

    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;

    mu_mmgr_lock_alloc();

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] ENTER: mmgrAllocator()");
#endif

    mu_mmgr_currentAllocationCount++;

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[+] %05u %8s of size 0x%08X(%08u) by %s", mu_mmgr_currentAllocationCount,
                    mu_mmgr_allocationTypes[allocationType], (unsigned)reportedSize, (unsigned)reportedSize,
                    mu_mmgr_ownerString(sourceFile, sourceLine, sourceFunc));
    }

    MMGR_ASSERT(mu_mmgr_currentAllocationCount != mu_mmgr_breakOnAllocationCount);

    if(!mu_mmgr_reservoir)
    {
        mu_mmgr_reservoir = (sAllocUnit*)malloc(sizeof(sAllocUnit) * 256u);
        MMGR_ASSERT(mu_mmgr_reservoir != NULL);

        if(!mu_mmgr_reservoir)
        {
            mu_mmgr_unlock_alloc();
            MMGR_ASSERT(false && "Unable to allocate RAM for internal memory tracking data");
            return NULL;
        }

        memset(mu_mmgr_reservoir, 0, sizeof(sAllocUnit) * 256u);
        for(unsigned int i = 0; i < 255u; ++i)
        {
            mu_mmgr_reservoir[i].next = &mu_mmgr_reservoir[i + 1u];
        }

        sAllocUnit** temp =
            (sAllocUnit**)realloc(mu_mmgr_reservoirBuffer, (mu_mmgr_reservoirBufferSize + 1u) * sizeof(sAllocUnit*));
        MMGR_ASSERT(temp != NULL);
        if(temp)
        {
            mu_mmgr_reservoirBuffer                                = temp;
            mu_mmgr_reservoirBuffer[mu_mmgr_reservoirBufferSize++] = mu_mmgr_reservoir;
        }
    }

    MMGR_ASSERT(mu_mmgr_reservoir != NULL);

    sAllocUnit* au    = mu_mmgr_reservoir;
    mu_mmgr_reservoir = au->next;

    memset(au, 0, sizeof(*au));
    au->actualSize = mu_mmgr_calculateActualSize(reportedSize) + alignment;
#ifdef FORCE_EXACT_ALIGNMENT
    au->actualSize += alignment;
#endif

#ifdef RANDOM_FAILURE
    {
        double a = (double)rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if(a > b)
        {
            au->actualAddress = malloc(au->actualSize);
        }
        else
        {
            mu_mmgr_log("[F] Random failure");
            au->actualAddress = NULL;
        }
    }
#else
    au->actualAddress = malloc(au->actualSize);
#endif

    au->reportedSize     = reportedSize;
    au->reportedAddress  = mu_mmgr_calculateReportedAddress(au->actualAddress);
    au->alignment        = alignment;
    au->allocationType   = allocationType;
    au->sourceLine       = sourceLine;
    au->allocationNumber = mu_mmgr_currentAllocationCount;

    size_t offset = ((size_t)au->reportedAddress) % alignment;
    if(offset)
    {
        au->reportedAddress = (uint8_t*)au->reportedAddress + (alignment - offset);
    }

#ifdef FORCE_EXACT_ALIGNMENT
    if(!((size_t)au->reportedAddress & alignment))
    {
        au->reportedAddress = (uint8_t*)au->reportedAddress + alignment;
        offset += alignment;
    }
#endif

    au->offset = offset;

    if(sourceFile)
    {
        mu_mmgr_strncpy(au->sourceFile, sizeof(au->sourceFile), mu_mmgr_sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1u);
    }
    else
    {
        mu_mmgr_strcpy(au->sourceFile, sizeof(au->sourceFile), "??");
    }

    if(sourceFunc)
    {
        mu_mmgr_strncpy(au->sourceFunc, sizeof(au->sourceFunc), sourceFunc, sizeof(au->sourceFunc) - 1u);
    }
    else
    {
        mu_mmgr_strcpy(au->sourceFunc, sizeof(au->sourceFunc), "??");
    }

#if MMGR_BACKTRACE
#if defined(_WIN32)
    au->backtrace_nptrs = CaptureStackBackTrace(mu_mmgr_stackSkipCount + 1, MMGR_BACKTRACE_SIZE, au->backtrace_buffer, NULL);
    au->backtrace_skip = 0;
#else
    au->backtrace_nptrs = backtrace(au->backtrace_buffer, MMGR_BACKTRACE_SIZE);
    au->backtrace_skip  = mu_mmgr_stackSkipCount + 1;
#endif
#endif

#ifndef RANDOM_FAILURE
    MMGR_ASSERT(au->actualAddress != NULL);
#endif

    if(!au->actualAddress)
    {
        mu_mmgr_unlock_alloc();
        MMGR_ASSERT(false && "Request for allocation failed. Out of memory.");
        return NULL;
    }

    MMGR_ASSERT(allocationType != m_alloc_unknown);

    size_t hashIndex = (((size_t)au->reportedAddress) >> 4u) & (mu_mmgr_hashSize - 1u);
    if(mu_mmgr_hashTable[hashIndex])
    {
        mu_mmgr_hashTable[hashIndex]->prev = au;
    }
    au->next                     = mu_mmgr_hashTable[hashIndex];
    au->prev                     = NULL;
    mu_mmgr_hashTable[hashIndex] = au;

    mu_mmgr_stats.totalReportedMemory += (unsigned int)au->reportedSize;
    mu_mmgr_stats.totalActualMemory += (unsigned int)au->actualSize;
    mu_mmgr_stats.totalAllocUnitCount++;
    if(mu_mmgr_stats.totalReportedMemory > mu_mmgr_stats.peakReportedMemory)
    {
        mu_mmgr_stats.peakReportedMemory = mu_mmgr_stats.totalReportedMemory;
    }
    if(mu_mmgr_stats.totalActualMemory > mu_mmgr_stats.peakActualMemory)
    {
        mu_mmgr_stats.peakActualMemory = mu_mmgr_stats.totalActualMemory;
    }
    if(mu_mmgr_stats.totalAllocUnitCount > mu_mmgr_stats.peakAllocUnitCount)
    {
        mu_mmgr_stats.peakAllocUnitCount = mu_mmgr_stats.totalAllocUnitCount;
    }
    mu_mmgr_stats.accumulatedReportedMemory += (unsigned int)au->reportedSize;
    mu_mmgr_stats.accumulatedActualMemory += (unsigned int)au->actualSize;
    mu_mmgr_stats.accumulatedAllocUnitCount++;

    mu_mmgr_wipeWithPattern(au, mu_mmgr_unusedPattern, 0);

    if(allocationType == m_alloc_calloc)
    {
        memset(au->reportedAddress, 0, au->reportedSize);
    }

    if(mu_mmgr_alwaysValidateAll)
    {
        mmgrValidateAllAllocUnits();
    }

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[+] ---->             addr 0x%08zX", (size_t)au->reportedAddress);
    }

    mu_mmgr_resetGlobals();

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] EXIT : mmgrAllocator()");
#endif

    mu_mmgr_unlock_alloc();
    return au->reportedAddress;
}

void* mmgrReallocator(const char*        sourceFile,
                      const unsigned int sourceLine,
                      const char*        sourceFunc,
                      const unsigned int reallocationType,
                      size_t             reportedSize,
                      void*              reportedAddress)
{
    reportedSize += sizeof(uint32_t) - 1u;
    reportedSize &= ~(sizeof(uint32_t) - 1u);

    mu_mmgr_lock_alloc();

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] ENTER: mmgrReallocator()");
#endif

    if(!reportedAddress)
    {
        mu_mmgr_unlock_alloc();
        return mmgrAllocator(sourceFile, sourceLine, sourceFunc, reallocationType, sizeof(void*), reportedSize);
    }

    mu_mmgr_currentAllocationCount++;
    MMGR_ASSERT(mu_mmgr_currentAllocationCount != mu_mmgr_breakOnAllocationCount);

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[~] %05u %8s of size 0x%08X(%08u) by %s", mu_mmgr_currentAllocationCount,
                    mu_mmgr_allocationTypes[reallocationType], (unsigned)reportedSize, (unsigned)reportedSize,
                    mu_mmgr_ownerString(sourceFile, sourceLine, sourceFunc));
    }

    sAllocUnit* au = mu_mmgr_findAllocUnit(reportedAddress);
    MMGR_ASSERT(au != NULL);
    if(!au)
    {
        mu_mmgr_unlock_alloc();
        MMGR_ASSERT(false && "Request to reallocate RAM that was never allocated");
        return NULL;
    }

    size_t alignment       = au->alignment;
    size_t oldReportedSize = au->reportedSize;

    MMGR_ASSERT(mmgrValidateAllocUnit(au));
    MMGR_ASSERT(reallocationType != m_alloc_unknown);
    MMGR_ASSERT(au->allocationType == m_alloc_malloc || au->allocationType == m_alloc_calloc || au->allocationType == m_alloc_realloc);
    MMGR_ASSERT(au->breakOnRealloc == false);

    unsigned int originalReportedSize = (unsigned int)au->reportedSize;

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[~] ---->             from 0x%08X(%08u)", originalReportedSize, originalReportedSize);
    }

    void*  oldReportedAddress = reportedAddress;
    size_t newActualSize      = mu_mmgr_calculateActualSize(reportedSize) + alignment;
#ifdef FORCE_EXACT_ALIGNMENT
    newActualSize += alignment;
#endif

    size_t minReportedSize = oldReportedSize < reportedSize ? oldReportedSize : reportedSize;
    void*  oldData         = malloc(minReportedSize ? minReportedSize : 1u);
    if(oldData && minReportedSize)
    {
        memcpy(oldData, oldReportedAddress, minReportedSize);
    }

    void* newActualAddress = NULL;
#ifdef RANDOM_FAILURE
    {
        double a = (double)rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if(a > b)
        {
            newActualAddress = realloc(au->actualAddress, newActualSize);
        }
        else
        {
            mu_mmgr_log("[F] Random failure");
        }
    }
#else
    newActualAddress = realloc(au->actualAddress, newActualSize);
#endif

#ifndef RANDOM_FAILURE
    MMGR_ASSERT(newActualAddress != NULL);
#endif

    if(!newActualAddress)
    {
        if(oldData)
        {
            free(oldData);
        }
        mu_mmgr_unlock_alloc();
        MMGR_ASSERT(false && "Request for reallocation failed. Out of memory.");
        return NULL;
    }

    mu_mmgr_stats.totalReportedMemory -= (unsigned int)au->reportedSize;
    mu_mmgr_stats.totalActualMemory -= (unsigned int)au->actualSize;

    au->actualSize       = newActualSize;
    au->actualAddress    = newActualAddress;
    au->reportedSize     = reportedSize;
    au->reportedAddress  = mu_mmgr_calculateReportedAddress(newActualAddress);
    au->allocationType   = reallocationType;
    au->sourceLine       = sourceLine;
    au->allocationNumber = mu_mmgr_currentAllocationCount;

    size_t offset = ((size_t)au->reportedAddress) % alignment;
    if(offset)
    {
        au->reportedAddress = (uint8_t*)au->reportedAddress + (alignment - offset);
    }

#ifdef FORCE_EXACT_ALIGNMENT
    if(!((size_t)au->reportedAddress & alignment))
    {
        au->reportedAddress = (uint8_t*)au->reportedAddress + alignment;
        offset += alignment;
    }
#endif

    if(offset != au->offset)
    {
        size_t size = oldReportedSize < reportedSize ? oldReportedSize : reportedSize;
        if(oldData && size)
        {
            memcpy(au->reportedAddress, oldData, size);
        }
        au->offset = offset;
    }

    if(oldData)
    {
        free(oldData);
    }

    if(sourceFile)
    {
        mu_mmgr_strncpy(au->sourceFile, sizeof(au->sourceFile), mu_mmgr_sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1u);
    }
    else
    {
        mu_mmgr_strcpy(au->sourceFile, sizeof(au->sourceFile), "??");
    }

    if(sourceFunc)
    {
        mu_mmgr_strncpy(au->sourceFunc, sizeof(au->sourceFunc), sourceFunc, sizeof(au->sourceFunc) - 1u);
    }
    else
    {
        mu_mmgr_strcpy(au->sourceFunc, sizeof(au->sourceFunc), "??");
    }

#if MMGR_BACKTRACE
#if defined(_WIN32)
    au->backtrace_nptrs = CaptureStackBackTrace(mu_mmgr_stackSkipCount + 1, MMGR_BACKTRACE_SIZE, au->backtrace_buffer, NULL);
    au->backtrace_skip = 0;
#else
    au->backtrace_nptrs = backtrace(au->backtrace_buffer, MMGR_BACKTRACE_SIZE);
    au->backtrace_skip  = mu_mmgr_stackSkipCount + 1;
#endif
#endif

    if(oldReportedAddress != au->reportedAddress)
    {
        size_t oldHash = (((size_t)oldReportedAddress) >> 4u) & (mu_mmgr_hashSize - 1u);
        if(mu_mmgr_hashTable[oldHash] == au)
        {
            mu_mmgr_hashTable[oldHash] = mu_mmgr_hashTable[oldHash]->next;
        }
        else
        {
            if(au->prev)
            {
                au->prev->next = au->next;
            }
            if(au->next)
            {
                au->next->prev = au->prev;
            }
        }

        size_t newHash = (((size_t)au->reportedAddress) >> 4u) & (mu_mmgr_hashSize - 1u);
        if(mu_mmgr_hashTable[newHash])
        {
            mu_mmgr_hashTable[newHash]->prev = au;
        }
        au->next                   = mu_mmgr_hashTable[newHash];
        au->prev                   = NULL;
        mu_mmgr_hashTable[newHash] = au;
    }

    mu_mmgr_stats.totalReportedMemory += (unsigned int)au->reportedSize;
    mu_mmgr_stats.totalActualMemory += (unsigned int)au->actualSize;
    if(mu_mmgr_stats.totalReportedMemory > mu_mmgr_stats.peakReportedMemory)
    {
        mu_mmgr_stats.peakReportedMemory = mu_mmgr_stats.totalReportedMemory;
    }
    if(mu_mmgr_stats.totalActualMemory > mu_mmgr_stats.peakActualMemory)
    {
        mu_mmgr_stats.peakActualMemory = mu_mmgr_stats.totalActualMemory;
    }
    {
        int deltaReportedSize = (int)(reportedSize - originalReportedSize);
        if(deltaReportedSize > 0)
        {
            mu_mmgr_stats.accumulatedReportedMemory += (unsigned int)deltaReportedSize;
            mu_mmgr_stats.accumulatedActualMemory += (unsigned int)deltaReportedSize;
        }
    }

    mu_mmgr_wipeWithPattern(au, mu_mmgr_unusedPattern, originalReportedSize);

    MMGR_ASSERT(mmgrValidateAllocUnit(au));

    if(mu_mmgr_alwaysValidateAll)
    {
        mmgrValidateAllAllocUnits();
    }

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[~] ---->             addr 0x%08zX", (size_t)au->reportedAddress);
    }

    mu_mmgr_resetGlobals();

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] EXIT : mmgrReallocator()");
#endif

    mu_mmgr_unlock_alloc();
    return au->reportedAddress;
}

void mmgrDeallocator(const char*        sourceFile,
                     const unsigned int sourceLine,
                     const char*        sourceFunc,
                     const unsigned int deallocationType,
                     const void*        reportedAddress)
{
    mu_mmgr_lock_alloc();

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] ENTER: mmgrDeallocator()");
#endif

    if(mu_mmgr_alwaysLogAll)
    {
        mu_mmgr_log("[-] ----- %8s of addr 0x%08zX           by %s", mu_mmgr_allocationTypes[deallocationType],
                    (size_t)((void*)reportedAddress), mu_mmgr_ownerString(sourceFile, sourceLine, sourceFunc));
    }

    if(reportedAddress)
    {
        sAllocUnit* au = mu_mmgr_findAllocUnit(reportedAddress);

        MMGR_ASSERT(au != NULL);
        if(!au)
        {
            mu_mmgr_unlock_alloc();
            MMGR_ASSERT(false && "Request to deallocate RAM that was never allocated");
            return;
        }

        MMGR_ASSERT(mmgrValidateAllocUnit(au));
        MMGR_ASSERT(deallocationType != m_alloc_unknown);
        MMGR_ASSERT((deallocationType == m_alloc_delete && au->allocationType == m_alloc_new)
                    || (deallocationType == m_alloc_delete_array && au->allocationType == m_alloc_new_array)
                    || (deallocationType == m_alloc_free && au->allocationType == m_alloc_malloc)
                    || (deallocationType == m_alloc_free && au->allocationType == m_alloc_calloc)
                    || (deallocationType == m_alloc_free && au->allocationType == m_alloc_realloc)
                    || (deallocationType == m_alloc_unknown));
        MMGR_ASSERT(au->breakOnDealloc == false);

        mu_mmgr_wipeWithPattern(au, mu_mmgr_releasedPattern, 0);

        free(au->actualAddress);

        size_t hashIndex = ((size_t)au->reportedAddress >> 4u) & (mu_mmgr_hashSize - 1u);
        if(mu_mmgr_hashTable[hashIndex] == au)
        {
            mu_mmgr_hashTable[hashIndex] = au->next;
        }
        else
        {
            if(au->prev)
            {
                au->prev->next = au->next;
            }
            if(au->next)
            {
                au->next->prev = au->prev;
            }
        }

        mu_mmgr_stats.totalReportedMemory -= (unsigned int)au->reportedSize;
        mu_mmgr_stats.totalActualMemory -= (unsigned int)au->actualSize;
        mu_mmgr_stats.totalAllocUnitCount--;

        memset(au, 0, sizeof(*au));
        au->next          = mu_mmgr_reservoir;
        mu_mmgr_reservoir = au;
    }

    mu_mmgr_resetGlobals();

    if(mu_mmgr_alwaysValidateAll)
    {
        mmgrValidateAllAllocUnits();
    }

#ifdef TEST_MEMORY_MANAGER
    mu_mmgr_log("[D] EXIT : mmgrDeallocator()");
#endif

    mu_mmgr_unlock_alloc();
}

bool* mmgrBreakOnRealloc(void* reportedAddress)
{
    sAllocUnit* au = mu_mmgr_findAllocUnit(reportedAddress);
    MMGR_ASSERT(au != NULL);
    MMGR_ASSERT(au->allocationType == m_alloc_malloc || au->allocationType == m_alloc_calloc || au->allocationType == m_alloc_realloc);
    return &au->breakOnRealloc;
}

bool* mmgrBreakOnDealloc(void* reportedAddress)
{
    sAllocUnit* au = mu_mmgr_findAllocUnit(reportedAddress);
    MMGR_ASSERT(au != NULL);
    return &au->breakOnDealloc;
}

bool mmgrValidateAddress(const void* reportedAddress)
{
    return mu_mmgr_findAllocUnit(reportedAddress) != NULL;
}

bool mmgrValidateAllocUnit(const sAllocUnit* allocUnit)
{
    uint8_t* pre       = (uint8_t*)allocUnit->reportedAddress - mu_mmgr_paddingSize * sizeof(uint32_t);
    uint8_t* post      = (uint8_t*)allocUnit->reportedAddress + allocUnit->reportedSize;
    bool     errorFlag = false;

    size_t paddingBytes = mu_mmgr_paddingSize * sizeof(uint32_t);
    for(size_t i = 0; i < paddingBytes; ++i, ++pre, ++post)
    {
        uint8_t expectedPrefixByte = (uint8_t)((mu_mmgr_prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFFu);
        if(*pre != expectedPrefixByte)
        {
            mu_mmgr_log("[!] A memory allocation unit was corrupt because of an underrun:");
            mmgrDumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }
        MMGR_ASSERT(*pre == expectedPrefixByte);

        uint8_t expectedPostfixByte = (uint8_t)((mu_mmgr_postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFFu);
        if(*post != expectedPostfixByte)
        {
            mu_mmgr_log("[!] A memory allocation unit was corrupt because of an overrun:");
            mmgrDumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }
        MMGR_ASSERT(*post == expectedPostfixByte);
    }

    return !errorFlag;
}

bool mmgrValidateAllAllocUnits(void)
{
    unsigned int errors     = 0;
    unsigned int allocCount = 0;

    for(unsigned int i = 0; i < mu_mmgr_hashSize; ++i)
    {
        sAllocUnit* ptr = mu_mmgr_hashTable[i];
        while(ptr)
        {
            allocCount++;
            if(!mmgrValidateAllocUnit(ptr))
            {
                errors++;
            }
            ptr = ptr->next;
        }
    }

    if(allocCount != mu_mmgr_stats.totalAllocUnitCount)
    {
        mu_mmgr_log("[!] Memory tracking hash table corrupt!");
        errors++;
    }

    MMGR_ASSERT(allocCount == mu_mmgr_stats.totalAllocUnitCount);
    MMGR_ASSERT(errors == 0);

    if(errors)
    {
        mu_mmgr_log("[!] While validating all allocation units, %u allocation unit(s) were found to have problems", errors);
    }

    return errors != 0;
}

unsigned int mmgrCalcUnused(const sAllocUnit* allocUnit)
{
    const uint32_t* ptr   = (const uint32_t*)allocUnit->reportedAddress;
    unsigned int    count = 0;

    for(unsigned int i = 0; i < allocUnit->reportedSize; i += (unsigned int)sizeof(uint32_t), ++ptr)
    {
        if(*ptr == mu_mmgr_unusedPattern)
        {
            count += (unsigned int)sizeof(uint32_t);
        }
    }

    return count;
}

unsigned int mmgrCalcAllUnused(void)
{
    unsigned int total = 0;
    for(unsigned int i = 0; i < mu_mmgr_hashSize; ++i)
    {
        sAllocUnit* ptr = mu_mmgr_hashTable[i];
        while(ptr)
        {
            total += mmgrCalcUnused(ptr);
            ptr = ptr->next;
        }
    }
    return total;
}

void mmgrSetExecutableName(const char* name, size_t length)
{
    if(!name)
    {
        mu_mmgr_executableName[0] = '\0';
        return;
    }

    size_t copyLen = length;
    if(copyLen >= sizeof(mu_mmgr_executableName))
    {
        copyLen = sizeof(mu_mmgr_executableName) - 1u;
    }
    memcpy(mu_mmgr_executableName, name, copyLen);
    mu_mmgr_executableName[copyLen] = '\0';

    if(!mu_mmgr_appName || mu_mmgr_appName[0] == '\0')
    {
        mu_mmgr_appName = mu_mmgr_executableName;
    }
}

void mmgrSetLogFileDirectory(const char* directory)
{
    if(!directory)
    {
        mu_mmgr_logDirectory[0] = '\0';
        return;
    }
    mu_mmgr_strcpy(mu_mmgr_logDirectory, sizeof(mu_mmgr_logDirectory), directory);
}

void mmgrDumpAllocUnit(const sAllocUnit* allocUnit, const char* prefix)
{
    const char* p = prefix ? prefix : "";
    mu_mmgr_log("[I] %sAddress (reported): %010p", p, allocUnit->reportedAddress);
    mu_mmgr_log("[I] %sAddress (actual)  : %010p", p, allocUnit->actualAddress);
    mu_mmgr_log("[I] %sSize (reported)   : 0x%08X (%s)", p, (unsigned int)allocUnit->reportedSize,
                mu_mmgr_memorySizeString((unsigned int)allocUnit->reportedSize));
    mu_mmgr_log("[I] %sSize (actual)     : 0x%08X (%s)", p, (unsigned int)allocUnit->actualSize,
                mu_mmgr_memorySizeString((unsigned int)allocUnit->actualSize));
    mu_mmgr_log("[I] %sOwner             : %s(%u)::%s", p, allocUnit->sourceFile, allocUnit->sourceLine, allocUnit->sourceFunc);
#if MMGR_BACKTRACE
    mu_mmgr_log("[I] %sBacktrace         :", p);
    mu_mmgr_dumpBacktrace(NULL, allocUnit);
#endif
    mu_mmgr_log("[I] %sAllocation type   : %s", p, mu_mmgr_allocationTypes[allocUnit->allocationType]);
    mu_mmgr_log("[I] %sAllocation number : %u", p, allocUnit->allocationNumber);
}

void mmgrDumpMemoryReport(const char* filename, const bool overwrite)
{
    if(!filename)
    {
        return;
    }

    char outputPath[MU_MMGR_PATH_MAX];
    mu_mmgr_buildPath(outputPath, sizeof(outputPath), filename);

    FILE* fh = fopen(outputPath, overwrite ? "wb" : "ab");
    if(!fh)
    {
        return;
    }

    time_t    t = time(NULL);
    struct tm tme;
    memset(&tme, 0, sizeof(tme));
    (void)mu_mmgr_localtime_safe(&t, &tme);

    fprintf(fh, " ----------------------------------------------------------------------------------------------------------------------------- -----\n");
    fprintf(fh, "|                                             Memory report for: %02d/%02d/%04d %02d:%02d:%02d                                           |\n",
            tme.tm_mon + 1, tme.tm_mday, tme.tm_year + 1900, tme.tm_hour, tme.tm_min, tme.tm_sec);
    fprintf(fh, " ----------------------------------------------------------------------------------------------------------------------------- -----\n\n");

    fprintf(fh, "Allocation unit count: %10s\n", mu_mmgr_insertCommas(mu_mmgr_stats.totalAllocUnitCount));
    fprintf(fh, "Reported to application: %s\n", mu_mmgr_memorySizeString(mu_mmgr_stats.totalReportedMemory));
    fprintf(fh, "Actual total memory in use: %s\n", mu_mmgr_memorySizeString(mu_mmgr_stats.totalActualMemory));
    fprintf(fh, "Memory tracking overhead: %s\n",
            mu_mmgr_memorySizeString(mu_mmgr_stats.totalActualMemory - mu_mmgr_stats.totalReportedMemory));
    fprintf(fh, "\n");

    mu_mmgr_dumpAllocations(fh);

    fclose(fh);
}

sMStats mmgrGetMemoryStatistics(void)
{
    return mu_mmgr_stats;
}

#endif /* MU_MMGR_IMPLEMENTATION */
