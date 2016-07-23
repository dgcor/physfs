/*****************************************************************************/
/* SListFile.cpp                          Copyright (c) Ladislav Zezula 2004 */
/*---------------------------------------------------------------------------*/
/* Description:                                                              */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 12.06.04  1.00  Lad  The first version of SListFile.cpp                   */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"
#include <assert.h>

//-----------------------------------------------------------------------------
// Listfile entry structure

#define CACHE_BUFFER_SIZE  0x1000       // Size of the cache buffer
#define MAX_LISTFILE_SIZE  0x04000000   // Maximum accepted listfile size is about 68 MB

struct TListFileCache
{
    char * szWildCard;                  // Self-relative pointer to file mask
    LPBYTE pBegin;                      // The begin of the listfile cache
    LPBYTE pPos;                        // Current position in the cache
    LPBYTE pEnd;                        // The last character in the file cache

//  char szWildCard[wildcard_length];   // Followed by the name mask (if any)
//  char szListFile[listfile_length];   // Followed by the listfile (if any)
};

//-----------------------------------------------------------------------------
// Local functions (cache)

static bool FreeListFileCache(TListFileCache * pCache)
{
    // Valid parameter check
    if(pCache != NULL)
        STORM_FREE(pCache);
    return true;
}

static TListFileCache * CreateListFileCache(HANDLE hListFile, const char * szWildCard)
{
    TListFileCache * pCache = NULL;
    size_t cchWildCard = 0;
    DWORD dwBytesRead = 0;
    DWORD dwFileSize;

    // Get the amount of bytes that need to be allocated
    dwFileSize = SFileGetFileSize(hListFile, NULL);
    if(dwFileSize == 0 || dwFileSize > MAX_LISTFILE_SIZE)
        return NULL;

    // Append buffer for name mask, if any
    if(szWildCard != NULL)
        cchWildCard = strlen(szWildCard) + 1;

    // Allocate cache for one file block
    pCache = (TListFileCache *)STORM_ALLOC(BYTE, sizeof(TListFileCache) + cchWildCard + dwFileSize + 1);
    if(pCache != NULL)
    {
        // Clear the entire structure
        memset(pCache, 0, sizeof(TListFileCache) + cchWildCard);

        // Shall we copy the mask?
        if(cchWildCard != 0)
        {
            pCache->szWildCard = (char *)(pCache + 1);
            memcpy(pCache->szWildCard, szWildCard, cchWildCard);
        }
                          
        // Fill-in the rest of the cache pointers
        pCache->pBegin = (LPBYTE)(pCache + 1) + cchWildCard;

        // Load the entire listfile to the cache
        SFileReadFile(hListFile, pCache->pBegin, dwFileSize, &dwBytesRead, NULL);
        if(dwBytesRead != 0)
        {
            // Allocate pointers
            pCache->pPos = pCache->pBegin;
            pCache->pEnd = pCache->pBegin + dwBytesRead;
        }
        else
        {
            FreeListFileCache(pCache);
            pCache = NULL;
        }
    }

    // Return the cache
    return pCache;
}

static char * ReadListFileLine(TListFileCache * pCache, size_t * PtrLength)
{
    LPBYTE pbLineBegin;
    LPBYTE pbLineEnd;
    LPBYTE pbExtraString = NULL;
    
    // Skip newlines, spaces, tabs and another non-printable stuff
    while(pCache->pPos < pCache->pEnd && pCache->pPos[0] <= 0x20)
        pCache->pPos++;
    
    // Set the line begin and end
    if(pCache->pPos >= pCache->pEnd)
        return NULL;
    pbLineBegin = pbLineEnd = pCache->pPos;

    // Copy the remaining characters
    while(pCache->pPos < pCache->pEnd && pCache->pPos[0] != 0x0A && pCache->pPos[0] != 0x0D)
    {
        // Blizzard listfiles can also contain information about patch:
        // Pass1\Files\MacOS\unconditional\user\Background Downloader.app\Contents\Info.plist~Patch(Data#frFR#base-frFR,1326)
        if(pCache->pPos[0] == '~')
            pbExtraString = pCache->pPos;

        // Copy the character
        pCache->pPos++;
    }

    // If there was extra string after the file name, clear it
    if(pbExtraString != NULL)
    {
        if(pbExtraString[0] == '~' && pbExtraString[1] == 'P')
        {
            pbLineEnd = pbExtraString;
            pbLineEnd[0] = 0;
        }
    }
    else
    {
        pbLineEnd = pCache->pPos++;
        pbLineEnd[0] = 0;
    }

    // Give the line to the caller
    if(PtrLength != NULL)
        PtrLength[0] = (size_t)(pbLineEnd - pbLineBegin);
    return (char *)pbLineBegin;
}

//-----------------------------------------------------------------------------
// Local functions (listfile nodes)

// Adds a name into the list of all names. For each locale in the MPQ,
// one entry will be created
// If the file name is already there, does nothing.
static int SListFileCreateNodeForAllLocales(TMPQArchive * ha, const char * szFileName)
{
    TMPQHash * pFirstHash;
    TMPQHash * pHash;

    // If we have hash table, we use it
    if(ha->pHashTable != NULL)
    {
        // Go while we found something
        pFirstHash = pHash = GetFirstHashEntry(ha, szFileName);
        while(pHash != NULL)
        {
            // Allocate file name for the file entry
            AllocateFileName(ha, ha->pFileTable + MPQ_BLOCK_INDEX(pHash), szFileName);

            // Now find the next language version of the file
            pHash = GetNextHashEntry(ha, pFirstHash, pHash);
        }

        return ERROR_SUCCESS;
    }

    return ERROR_CAN_NOT_COMPLETE;
}

static int SFileAddArbitraryListFile(
    TMPQArchive * ha,
    HANDLE hListFile)
{
    TListFileCache * pCache = NULL;

    // Create the listfile cache for that file
    pCache = CreateListFileCache(hListFile, NULL);
    if(pCache != NULL)
    {
        char * szFileName;
        size_t nLength = 0;

        // Get the next line
        while((szFileName = ReadListFileLine(pCache, &nLength)) != NULL)
        {
            // Add the line to the MPQ
            if(nLength != 0)
                SListFileCreateNodeForAllLocales(ha, szFileName);
        }

        // Delete the cache
        FreeListFileCache(pCache);
    }
    
    return (pCache != NULL) ? ERROR_SUCCESS : ERROR_FILE_CORRUPT;
}

static int SFileAddExternalListFile(
    TMPQArchive * ha,
    HANDLE hMpq,
    const char * szListFile)
{
    HANDLE hListFile;
    int nError = ERROR_SUCCESS;

    // Open the external list file
    if(!SFileOpenFileEx(hMpq, szListFile, SFILE_OPEN_LOCAL_FILE, &hListFile))
        return GetLastError();

    // Add the data from the listfile to MPQ
    nError = SFileAddArbitraryListFile(ha, hListFile);
    SFileCloseFile(hListFile);
    return nError;
}

static int SFileAddInternalListFile(
    TMPQArchive * ha,
    HANDLE hMpq)
{
    TMPQHash * pFirstHash;
    TMPQHash * pHash;
    HANDLE hListFile;
    DWORD dwFileSize;
    LCID lcSaveLocale = lcFileLocale;
    bool bIgnoreListFile = false;
    int nError = ERROR_SUCCESS;

    // If there is hash table, we need to support multiple listfiles
    // with different locales (BrooDat.mpq)
    if(ha->pHashTable != NULL)
    {
        pFirstHash = pHash = GetFirstHashEntry(ha, LISTFILE_NAME);
        while(nError == ERROR_SUCCESS && pHash != NULL)
        {                                
            // Set the prefered locale to that from list file
            SFileSetLocale(pHash->lcLocale);
            
            // Attempt to open the file with that locale
            if(SFileOpenFileEx(hMpq, LISTFILE_NAME, 0, &hListFile))
            {
                // If the archive is a malformed map, ignore too large listfiles
                if(ha->dwFlags & MPQ_FLAG_MALFORMED)
                {
                    dwFileSize = SFileGetFileSize(hListFile, NULL);
                    bIgnoreListFile = (dwFileSize > 0x40000);
                }

                // Add the data from the listfile to MPQ
                if(bIgnoreListFile == false)
                    nError = SFileAddArbitraryListFile(ha, hListFile);
                SFileCloseFile(hListFile);
            }
            
            // Restore the original locale
            SFileSetLocale(lcSaveLocale);

            // Move to the next hash
            pHash = GetNextHashEntry(ha, pFirstHash, pHash);
        }
    }
    else
    {
        // Open the external list file
        if(SFileOpenFileEx(hMpq, LISTFILE_NAME, 0, &hListFile))
        {
            // Add the data from the listfile to MPQ
            // The function also closes the listfile handle
            nError = SFileAddArbitraryListFile(ha, hListFile);
            SFileCloseFile(hListFile);
        }
    }

    // Return the result of the operation
    return nError;
}

static bool DoListFileSearch(TListFileCache * pCache, SFILE_FIND_DATA * lpFindFileData)
{
    // Check for the valid search handle
    if(pCache != NULL)
    {
        char * szFileName;
        size_t nLength = 0;

        // Get the next line
        while((szFileName = ReadListFileLine(pCache, &nLength)) != NULL)
        {
            // Check search mask
            if(nLength != 0 && CheckWildCard(szFileName, pCache->szWildCard))
            {
                if(nLength >= sizeof(lpFindFileData->cFileName))
                    nLength = sizeof(lpFindFileData->cFileName) - 1;

                memcpy(lpFindFileData->cFileName, szFileName, nLength);
                lpFindFileData->cFileName[nLength] = 0;
                return true;
            }
        }
    }

    // No more files
    memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));
    SetLastError(ERROR_NO_MORE_FILES);
    return false;
}

//-----------------------------------------------------------------------------
// File functions

// Adds a listfile into the MPQ archive.
int WINAPI SFileAddListFile(HANDLE hMpq, const char * szListFile)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    int nError = ERROR_SUCCESS;

    // Add the listfile for each MPQ in the patch chain
    while(ha != NULL)
    {
        if(szListFile != NULL)
            nError = SFileAddExternalListFile(ha, hMpq, szListFile);
        else
            nError = SFileAddInternalListFile(ha, hMpq);

        // Also, add three special files to the listfile:
        // (listfile) itself, (attributes) and (signature)
        SListFileCreateNodeForAllLocales(ha, LISTFILE_NAME);
        SListFileCreateNodeForAllLocales(ha, SIGNATURE_NAME);
        SListFileCreateNodeForAllLocales(ha, ATTRIBUTES_NAME);

        // Move to the next archive in the chain
        ha = ha->haPatch;
    }

    return nError;
}

//-----------------------------------------------------------------------------
// Enumerating files in listfile

HANDLE WINAPI SListFileFindFirstFile(HANDLE hMpq, const char * szListFile, const char * szMask, SFILE_FIND_DATA * lpFindFileData)
{
    TListFileCache * pCache = NULL;
    HANDLE hListFile = NULL;
    DWORD dwSearchScope = SFILE_OPEN_LOCAL_FILE;

    // Initialize the structure with zeros
    memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));

    // If the szListFile is NULL, it means we have to open internal listfile
    if(szListFile == NULL)
    {
        // Use SFILE_OPEN_ANY_LOCALE for listfile. This will allow us to load
        // the listfile even if there is only non-neutral version of the listfile in the MPQ
        dwSearchScope = SFILE_OPEN_ANY_LOCALE;
        szListFile = LISTFILE_NAME;
    }

    // Open the local/internal listfile
    if(SFileOpenFileEx(hMpq, szListFile, dwSearchScope, &hListFile))
    {
        pCache = CreateListFileCache(hListFile, szMask);
        SFileCloseFile(hListFile);
    }

    if(!DoListFileSearch(pCache, lpFindFileData))
    {
        memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));
        SetLastError(ERROR_NO_MORE_FILES);
        FreeListFileCache(pCache);
        pCache = NULL;
    }

    // Return the listfile cache as handle
    return (HANDLE)pCache;
}

bool WINAPI SListFileFindNextFile(HANDLE hFind, SFILE_FIND_DATA * lpFindFileData)
{
    return DoListFileSearch((TListFileCache *)hFind, lpFindFileData);
}

bool WINAPI SListFileFindClose(HANDLE hFind)
{
    TListFileCache * pCache = (TListFileCache *)hFind;

    return FreeListFileCache(pCache);
}

