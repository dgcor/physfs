/*****************************************************************************/
/* SCommon.h                              Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Common functions for encryption/decryption from Storm.dll. Included by    */
/* SFile*** functions, do not include and do not use this file directly      */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 24.03.03  1.00  Lad  The first version of SFileCommon.h                   */
/* 12.06.04  1.00  Lad  Renamed to SCommon.h                                 */
/* 06.09.10  1.00  Lad  Renamed to StormCommon.h                             */
/*****************************************************************************/

#ifndef __STORMCOMMON_H__
#define __STORMCOMMON_H__

//-----------------------------------------------------------------------------
// Compression support

// Include functions from Pkware Data Compression Library
#include "pklib/pklib.h"

//-----------------------------------------------------------------------------
// StormLib private defines

#define ID_MPQ_FILE            0x46494c45     // Used internally for checking TMPQFile ('FILE')

// Prevent problems with CRT "min" and "max" functions,
// as they are not defined on all platforms
#define STORMLIB_MIN(a, b) ((a < b) ? a : b)
#define STORMLIB_MAX(a, b) ((a > b) ? a : b)
#define STORMLIB_UNUSED(p) ((void)(p))

// Macro for building 64-bit file offset from two 32-bit
#define MAKE_OFFSET64(hi, lo)      (((ULONGLONG)hi << 32) | (ULONGLONG)lo)

//-----------------------------------------------------------------------------
// MPQ signature information

// Size of each signature type
#define MPQ_WEAK_SIGNATURE_SIZE                 64
#define MPQ_STRONG_SIGNATURE_SIZE              256 
#define MPQ_STRONG_SIGNATURE_ID         0x5349474E      // ID of the strong signature ("NGIS")
#define MPQ_SIGNATURE_FILE_SIZE (MPQ_WEAK_SIGNATURE_SIZE + 8)

//-----------------------------------------------------------------------------
// Memory management
//
// We use our own macros for allocating/freeing memory. If you want
// to redefine them, please keep the following rules:
//
//  - The memory allocation must return NULL if not enough memory
//    (i.e not to throw exception)
//  - The allocating function does not need to fill the allocated buffer with zeros
//  - Memory freeing function doesn't have to test the pointer to NULL
//

//#if defined(_MSC_VER) && defined(_DEBUG)
//
//#define STORM_ALLOC(type, nitems)        (type *)HeapAlloc(GetProcessHeap(), 0, ((nitems) * sizeof(type)))
//#define STORM_REALLOC(type, ptr, nitems) (type *)HeapReAlloc(GetProcessHeap(), 0, ptr, ((nitems) * sizeof(type)))
//#define STORM_FREE(ptr)                  HeapFree(GetProcessHeap(), 0, ptr)
//
//#else

#define STORM_ALLOC(type, nitems)        (type *)malloc((nitems) * sizeof(type))
#define STORM_REALLOC(type, ptr, nitems) (type *)realloc(ptr, ((nitems) * sizeof(type)))
#define STORM_FREE(ptr)                  free(ptr)

//#endif

//-----------------------------------------------------------------------------
// StormLib internal global variables

extern LCID lcFileLocale;                       // Preferred file locale

//-----------------------------------------------------------------------------
// Conversion to uppercase/lowercase (and "/" to "\")

extern unsigned char AsciiToLowerTable[256];
extern unsigned char AsciiToUpperTable[256];

//-----------------------------------------------------------------------------
// Safe string functions

void StringCopyA(char * dest, const char * src, size_t nMaxChars);
void StringCatA(char * dest, const char * src, size_t nMaxChars);

void StringCopyT(TCHAR * dest, const TCHAR * src, size_t nMaxChars);
void StringCatT(TCHAR * dest, const TCHAR * src, size_t nMaxChars);

//-----------------------------------------------------------------------------
// Encryption and decryption functions

#define MPQ_HASH_TABLE_INDEX    0x000
#define MPQ_HASH_NAME_A         0x100
#define MPQ_HASH_NAME_B         0x200
#define MPQ_HASH_FILE_KEY       0x300
#define MPQ_HASH_KEY2_MIX       0x400

DWORD HashString(const char * szFileName, DWORD dwHashType);
DWORD HashStringSlash(const char * szFileName, DWORD dwHashType);
DWORD HashStringLower(const char * szFileName, DWORD dwHashType);

void  InitializeMpqCryptography();

DWORD GetNearestPowerOfTwo(DWORD dwFileCount);

bool IsPseudoFileName(const char * szFileName, LPDWORD pdwFileIndex);

DWORD GetDefaultSpecialFileFlags(DWORD dwFileSize, USHORT wFormatVersion);

void  DecryptMpqBlock(void * pvDataBlock, DWORD dwLength, DWORD dwKey);

DWORD DetectFileKeyBySectorSize(LPDWORD EncryptedData, DWORD dwSectorSize, DWORD dwSectorOffsLen);
DWORD DetectFileKeyByContent(void * pvEncryptedData, DWORD dwSectorSize, DWORD dwFileSize);
DWORD DecryptFileKey(const char * szFileName, ULONGLONG MpqPos, DWORD dwFileSize, DWORD dwFlags);

//-----------------------------------------------------------------------------
// Handle validation functions

TMPQArchive * IsValidMpqHandle(HANDLE hMpq);
TMPQFile * IsValidFileHandle(HANDLE hFile);

//-----------------------------------------------------------------------------
// Support for MPQ file tables

ULONGLONG FileOffsetFromMpqOffset(TMPQArchive * ha, ULONGLONG MpqOffset);
ULONGLONG CalculateRawSectorOffset(TMPQFile * hf, DWORD dwSectorOffset);

int ConvertMpqHeaderToFormat4(TMPQArchive * ha, ULONGLONG MpqOffset, ULONGLONG FileSize, DWORD dwFlags);

bool IsValidHashEntry(TMPQArchive * ha, TMPQHash * pHash);

TMPQHash * FindFreeHashEntry(TMPQArchive * ha, DWORD dwStartIndex, DWORD dwName1, DWORD dwName2, LCID lcLocale);
TMPQHash * GetFirstHashEntry(TMPQArchive * ha, const char * szFileName);
TMPQHash * GetNextHashEntry(TMPQArchive * ha, TMPQHash * pFirstHash, TMPQHash * pPrevHash);
TMPQHash * AllocateHashEntry(TMPQArchive * ha, TFileEntry * pFileEntry, LCID lcLocale);

TMPQBlock * LoadBlockTable(TMPQArchive * ha, bool bDontFixEntries = false);
TMPQBlock * TranslateBlockTable(TMPQArchive * ha, ULONGLONG * pcbTableSize, bool * pbNeedHiBlockTable);

// Functions that load the HET and BET tables
int  CreateHashTable(TMPQArchive * ha, DWORD dwHashTableSize);
int  LoadAnyHashTable(TMPQArchive * ha);
int  BuildFileTable(TMPQArchive * ha);
int  DefragmentFileTable(TMPQArchive * ha);

int  CreateFileTable(TMPQArchive * ha, DWORD dwFileTableSize);
int  RebuildFileTable(TMPQArchive * ha, DWORD dwNewHashTableSize);

// Functions for finding files in the file table
TFileEntry * GetFileEntryLocale2(TMPQArchive * ha, const char * szFileName, LCID lcLocale, LPDWORD PtrHashIndex);
TFileEntry * GetFileEntryLocale(TMPQArchive * ha, const char * szFileName, LCID lcLocale);
TFileEntry * GetFileEntryExact(TMPQArchive * ha, const char * szFileName, LCID lcLocale, LPDWORD PtrHashIndex);

// Allocates file name in the file entry
void AllocateFileName(TMPQArchive * ha, TFileEntry * pFileEntry, const char * szFileName);

//-----------------------------------------------------------------------------
// Support for alternate file formats (SBaseSubTypes.cpp)

int ConvertSqpHeaderToFormat4(TMPQArchive * ha, ULONGLONG FileSize, DWORD dwFlags);

//-----------------------------------------------------------------------------
// Common functions - MPQ File

TMPQFile * CreateFileHandle(TMPQArchive * ha, TFileEntry * pFileEntry);
void * LoadMpqTable(TMPQArchive * ha, ULONGLONG ByteOffset, DWORD dwCompressedSize, DWORD dwRealSize, DWORD dwKey, bool * pbTableIsCut);
int  AllocateSectorBuffer(TMPQFile * hf);
int  AllocateSectorOffsets(TMPQFile * hf, bool bLoadFromFile);
int  AllocateSectorChecksums(TMPQFile * hf, bool bLoadFromFile);
void FreeFileHandle(TMPQFile *& hf);
void FreeArchiveHandle(TMPQArchive *& ha);

//-----------------------------------------------------------------------------
// Patch functions

// Structure used for the patching process
typedef struct _TMPQPatcher
{
    BYTE this_md5[MD5_DIGEST_SIZE];             // MD5 of the current file state
    LPBYTE pbFileData1;                         // Primary working buffer
    LPBYTE pbFileData2;                         // Secondary working buffer
    DWORD cbMaxFileData;                        // Maximum allowed size of the patch data
    DWORD cbFileData;                           // Current size of the result data
    DWORD nCounter;                             // Counter of the patch process

} TMPQPatcher;

//-----------------------------------------------------------------------------
// Utility functions

bool IsInternalMpqFileName(const char * szFileName);

const TCHAR * GetPlainFileName(const TCHAR * szFileName);
const char * GetPlainFileName(const char * szFileName);

void CopyFileName(TCHAR * szTarget, const char * szSource, size_t cchLength);
void CopyFileName(char * szTarget, const TCHAR * szSource, size_t cchLength);

//-----------------------------------------------------------------------------
// Dump data support

#ifdef __STORMLIB_DUMP_DATA__

void DumpMpqHeader(TMPQHeader * pHeader);
void DumpHashTable(TMPQHash * pHashTable, DWORD dwHashTableSize);
void DumpHetAndBetTable(TMPQHetTable * pHetTable, TMPQBetTable * pBetTable);
void DumpFileTable(TFileEntry * pFileTable, DWORD dwFileTableSize);

#else

#define DumpMpqHeader(h)            /* */
#define DumpHashTable(t, s)         /* */
#define DumpHetAndBetTable(t, s)    /* */
#define DumpFileTable(t, s)         /* */

#endif

#endif // __STORMCOMMON_H__

