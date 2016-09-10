/*****************************************************************************/
/* FileStream.cpp                         Copyright (c) Ladislav Zezula 2010 */
/*---------------------------------------------------------------------------*/
/* File stream support for StormLib                                          */
/*                                                                           */
/* Windows support: Written by Ladislav Zezula                               */
/* Mac support:     Written by Sam Wilkins                                   */
/* Linux support:   Written by Sam Wilkins and Ivan Komissarov               */
/* Big-endian:      Written & debugged by Sam Wilkins                        */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 11.06.10  1.00  Lad  Derived from StormPortMac.cpp and StormPortLinux.cpp */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"
#include "FileStream.h"

#ifdef _MSC_VER
#pragma comment(lib, "wininet.lib")             // Internet functions for HTTP stream
#pragma warning(disable: 4800)                  // 'BOOL' : forcing value to bool 'true' or 'false' (performance warning)
#endif

//-----------------------------------------------------------------------------
// Local defines

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#endif

//-----------------------------------------------------------------------------
// Local functions - platform-specific functions

#ifndef PLATFORM_WINDOWS
static int nLastError = ERROR_SUCCESS;

int GetLastError()
{
    return nLastError;
}

void SetLastError(int nError)
{
    nLastError = nError;
}
#endif

//-----------------------------------------------------------------------------
// Dummy init function

static void BaseNone_Init(TFileStream *)
{
    // Nothing here
}

//-----------------------------------------------------------------------------
// Local functions - base file support

static bool BaseFile_Create(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    {
        DWORD dwWriteShare = (pStream->dwFlags & STREAM_FLAG_WRITE_SHARE) ? FILE_SHARE_WRITE : 0;

        pStream->Base.File.hFile = CreateFile(pStream->szFileName,
                                              GENERIC_READ | GENERIC_WRITE,
                                              dwWriteShare | FILE_SHARE_READ,
                                              NULL,
                                              CREATE_ALWAYS,
                                              0,
                                              NULL);
        if(pStream->Base.File.hFile == INVALID_HANDLE_VALUE)
            return false;
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        intptr_t handle;
        
        handle = open(pStream->szFileName, O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(handle == -1)
        {
            nLastError = errno;
            return false;
        }
        
        pStream->Base.File.hFile = (HANDLE)handle;
    }
#endif

    // Reset the file size and position
    pStream->Base.File.FileSize = 0;
    pStream->Base.File.FilePos = 0;
    return true;
}

static bool BaseFile_Open(TFileStream * pStream, const TCHAR * szFileName, DWORD dwStreamFlags)
{
#ifdef PLATFORM_WINDOWS
    {
        ULARGE_INTEGER FileSize;
        DWORD dwWriteAccess = (dwStreamFlags & STREAM_FLAG_READ_ONLY) ? 0 : FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES;
        DWORD dwWriteShare = (dwStreamFlags & STREAM_FLAG_WRITE_SHARE) ? FILE_SHARE_WRITE : 0;

        // Open the file
        pStream->Base.File.hFile = CreateFile(szFileName,
                                              FILE_READ_DATA | FILE_READ_ATTRIBUTES | dwWriteAccess,
                                              FILE_SHARE_READ | dwWriteShare,
                                              NULL,
                                              OPEN_EXISTING,
                                              0,
                                              NULL);
        if(pStream->Base.File.hFile == INVALID_HANDLE_VALUE)
            return false;

        // Query the file size
        FileSize.LowPart = GetFileSize(pStream->Base.File.hFile, &FileSize.HighPart);
        pStream->Base.File.FileSize = FileSize.QuadPart;

        // Query last write time
        GetFileTime(pStream->Base.File.hFile, NULL, NULL, (LPFILETIME)&pStream->Base.File.FileTime);
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        struct stat64 fileinfo;
        int oflag = (dwStreamFlags & STREAM_FLAG_READ_ONLY) ? O_RDONLY : O_RDWR;
        intptr_t handle;

        // Open the file
        handle = open(szFileName, oflag | O_LARGEFILE);
        if(handle == -1)
        {
            nLastError = errno;
            return false;
        }

        // Get the file size
        if(fstat64(handle, &fileinfo) == -1)
        {
            nLastError = errno;
            return false;
        }

        // time_t is number of seconds since 1.1.1970, UTC.
        // 1 second = 10000000 (decimal) in FILETIME
        // Set the start to 1.1.1970 00:00:00
        pStream->Base.File.FileTime = 0x019DB1DED53E8000ULL + (10000000 * fileinfo.st_mtime);
        pStream->Base.File.FileSize = (ULONGLONG)fileinfo.st_size;
        pStream->Base.File.hFile = (HANDLE)handle;
    }
#endif

    // Reset the file position
    pStream->Base.File.FilePos = 0;
    return true;
}

static bool BaseFile_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->Base.File.FilePos;
    DWORD dwBytesRead = 0;                  // Must be set by platform-specific code

#ifdef PLATFORM_WINDOWS
    {
        // Note: StormLib no longer supports Windows 9x.
        // Thus, we can use the OVERLAPPED structure to specify
        // file offset to read from file. This allows us to skip
        // one system call to SetFilePointer

        // Update the byte offset
        pStream->Base.File.FilePos = ByteOffset;

        // Read the data
        if(dwBytesToRead != 0)
        {
            OVERLAPPED Overlapped;

            Overlapped.OffsetHigh = (DWORD)(ByteOffset >> 32);
            Overlapped.Offset = (DWORD)ByteOffset;
            Overlapped.hEvent = NULL;
            if(!ReadFile(pStream->Base.File.hFile, pvBuffer, dwBytesToRead, &dwBytesRead, &Overlapped))
                return false;
        }
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        ssize_t bytes_read;

        // If the byte offset is different from the current file position,
        // we have to update the file position   xxx
        if(ByteOffset != pStream->Base.File.FilePos)
        {
            lseek64((intptr_t)pStream->Base.File.hFile, (off64_t)(ByteOffset), SEEK_SET);
            pStream->Base.File.FilePos = ByteOffset;
        }

        // Perform the read operation
        if(dwBytesToRead != 0)
        {
            bytes_read = read((intptr_t)pStream->Base.File.hFile, pvBuffer, (size_t)dwBytesToRead);
            if(bytes_read == -1)
            {
                nLastError = errno;
                return false;
            }
            
            dwBytesRead = (DWORD)(size_t)bytes_read;
        }
    }
#endif

    // Increment the current file position by number of bytes read
    // If the number of bytes read doesn't match to required amount, return false
    pStream->Base.File.FilePos = ByteOffset + dwBytesRead;
    if(dwBytesRead != dwBytesToRead)
        SetLastError(ERROR_HANDLE_EOF);
    return (dwBytesRead == dwBytesToRead);
}

/**
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, writes to current position
 * \a pvBuffer Pointer to data to be written
 * \a dwBytesToWrite Number of bytes to write to the file
 */

static bool BaseFile_Write(TFileStream * pStream, ULONGLONG * pByteOffset, const void * pvBuffer, DWORD dwBytesToWrite)
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->Base.File.FilePos;
    DWORD dwBytesWritten = 0;               // Must be set by platform-specific code

#ifdef PLATFORM_WINDOWS
    {
        // Note: StormLib no longer supports Windows 9x.
        // Thus, we can use the OVERLAPPED structure to specify
        // file offset to read from file. This allows us to skip
        // one system call to SetFilePointer

        // Update the byte offset
        pStream->Base.File.FilePos = ByteOffset;

        // Read the data
        if(dwBytesToWrite != 0)
        {
            OVERLAPPED Overlapped;

            Overlapped.OffsetHigh = (DWORD)(ByteOffset >> 32);
            Overlapped.Offset = (DWORD)ByteOffset;
            Overlapped.hEvent = NULL;
            if(!WriteFile(pStream->Base.File.hFile, pvBuffer, dwBytesToWrite, &dwBytesWritten, &Overlapped))
                return false;
        }
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        ssize_t bytes_written;

        // If the byte offset is different from the current file position,
        // we have to update the file position
        if(ByteOffset != pStream->Base.File.FilePos)
        {
            lseek64((intptr_t)pStream->Base.File.hFile, (off64_t)(ByteOffset), SEEK_SET);
            pStream->Base.File.FilePos = ByteOffset;
        }

        // Perform the read operation
        bytes_written = write((intptr_t)pStream->Base.File.hFile, pvBuffer, (size_t)dwBytesToWrite);
        if(bytes_written == -1)
        {
            nLastError = errno;
            return false;
        }
        
        dwBytesWritten = (DWORD)(size_t)bytes_written;
    }
#endif

    // Increment the current file position by number of bytes read
    pStream->Base.File.FilePos = ByteOffset + dwBytesWritten;

    // Also modify the file size, if needed
    if(pStream->Base.File.FilePos > pStream->Base.File.FileSize)
        pStream->Base.File.FileSize = pStream->Base.File.FilePos;

    if(dwBytesWritten != dwBytesToWrite)
        SetLastError(ERROR_DISK_FULL);
    return (dwBytesWritten == dwBytesToWrite);
}

/**
 * \a pStream Pointer to an open stream
 * \a NewFileSize New size of the file
 */
static bool BaseFile_Resize(TFileStream * pStream, ULONGLONG NewFileSize)
{
#ifdef PLATFORM_WINDOWS
    {
        LONG FileSizeHi = (LONG)(NewFileSize >> 32);
        LONG FileSizeLo;
        DWORD dwNewPos;
        bool bResult;

        // Set the position at the new file size
        dwNewPos = SetFilePointer(pStream->Base.File.hFile, (LONG)NewFileSize, &FileSizeHi, FILE_BEGIN);
        if(dwNewPos == INVALID_SET_FILE_POINTER && GetLastError() != ERROR_SUCCESS)
            return false;

        // Set the current file pointer as the end of the file
        bResult = (bool)SetEndOfFile(pStream->Base.File.hFile);
        if(bResult)
            pStream->Base.File.FileSize = NewFileSize;

        // Restore the file position
        FileSizeHi = (LONG)(pStream->Base.File.FilePos >> 32);
        FileSizeLo = (LONG)(pStream->Base.File.FilePos);
        SetFilePointer(pStream->Base.File.hFile, FileSizeLo, &FileSizeHi, FILE_BEGIN);
        return bResult;
    }
#endif
    
#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        if(ftruncate((intptr_t)pStream->Base.File.hFile, (off64_t)NewFileSize) == -1)
        {
            nLastError = errno;
            return false;
        }
        
        pStream->Base.File.FileSize = NewFileSize;
        return true;
    }
#endif
}

// Gives the current file size
static bool BaseFile_GetSize(TFileStream * pStream, ULONGLONG * pFileSize)
{
    // Note: Used by all thre base providers.
    // Requires the TBaseData union to have the same layout for all three base providers
    *pFileSize = pStream->Base.File.FileSize;
    return true;
}

// Gives the current file position
static bool BaseFile_GetPos(TFileStream * pStream, ULONGLONG * pByteOffset)
{
    // Note: Used by all thre base providers.
    // Requires the TBaseData union to have the same layout for all three base providers
    *pByteOffset = pStream->Base.File.FilePos;
    return true;
}

// Renames the file pointed by pStream so that it contains data from pNewStream
static bool BaseFile_Replace(TFileStream * pStream, TFileStream * pNewStream)
{
#ifdef PLATFORM_WINDOWS
    // Delete the original stream file. Don't check the result value,
    // because if the file doesn't exist, it would fail
    DeleteFile(pStream->szFileName);

    // Rename the new file to the old stream's file
    return (bool)MoveFile(pNewStream->szFileName, pStream->szFileName);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    // "rename" on Linux also works if the target file exists
    if(rename(pNewStream->szFileName, pStream->szFileName) == -1)
    {
        nLastError = errno;
        return false;
    }
    
    return true;
#endif
}

static void BaseFile_Close(TFileStream * pStream)
{
    if(pStream->Base.File.hFile != INVALID_HANDLE_VALUE)
    {
#ifdef PLATFORM_WINDOWS
        CloseHandle(pStream->Base.File.hFile);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
        close((intptr_t)pStream->Base.File.hFile);
#endif
    }

    // Also invalidate the handle
    pStream->Base.File.hFile = INVALID_HANDLE_VALUE;
}

// Initializes base functions for the disk file
static void BaseFile_Init(TFileStream * pStream)
{
    pStream->BaseCreate  = BaseFile_Create;
    pStream->BaseOpen    = BaseFile_Open;
    pStream->BaseRead    = BaseFile_Read;
    pStream->BaseWrite   = BaseFile_Write;
    pStream->BaseResize  = BaseFile_Resize;
    pStream->BaseGetSize = BaseFile_GetSize;
    pStream->BaseGetPos  = BaseFile_GetPos;
    pStream->BaseClose   = BaseFile_Close;
}

//-----------------------------------------------------------------------------
// Local functions - base memory-mapped file support

static bool BaseMap_Open(TFileStream * pStream, const TCHAR * szFileName, DWORD dwStreamFlags)
{
#ifdef PLATFORM_WINDOWS

    ULARGE_INTEGER FileSize;
    HANDLE hFile;
    HANDLE hMap;
    bool bResult = false;

    // Keep compiler happy
    dwStreamFlags = dwStreamFlags;

    // Open the file for read access
    hFile = CreateFile(szFileName, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        // Retrieve file size. Don't allow mapping file of a zero size.
        FileSize.LowPart = GetFileSize(hFile, &FileSize.HighPart);
        if(FileSize.QuadPart != 0)
        {
            // Now create mapping object
            hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
            if(hMap != NULL)
            {
                // Map the entire view into memory
                // Note that this operation will fail if the file can't fit
                // into usermode address space
                pStream->Base.Map.pbFile = (LPBYTE)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
                if(pStream->Base.Map.pbFile != NULL)
                {
                    // Retrieve file time
                    GetFileTime(hFile, NULL, NULL, (LPFILETIME)&pStream->Base.Map.FileTime);

                    // Retrieve file size and position
                    pStream->Base.Map.FileSize = FileSize.QuadPart;
                    pStream->Base.Map.FilePos = 0;
                    bResult = true;
                }

                // Close the map handle
                CloseHandle(hMap);
            }
        }

        // Close the file handle
        CloseHandle(hFile);
    }

    // If the file is not there and is not available for random access,
    // report error
    if(bResult == false)
        return false;
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    struct stat64 fileinfo;
    intptr_t handle;
    bool bResult = false;

    // Open the file
    handle = open(szFileName, O_RDONLY);
    if(handle != -1)
    {
        // Get the file size
        if(fstat64(handle, &fileinfo) != -1)
        {
            pStream->Base.Map.pbFile = (LPBYTE)mmap(NULL, (size_t)fileinfo.st_size, PROT_READ, MAP_PRIVATE, handle, 0);
            if(pStream->Base.Map.pbFile != NULL)
            {
                // time_t is number of seconds since 1.1.1970, UTC.
                // 1 second = 10000000 (decimal) in FILETIME
                // Set the start to 1.1.1970 00:00:00
                pStream->Base.Map.FileTime = 0x019DB1DED53E8000ULL + (10000000 * fileinfo.st_mtime);
                pStream->Base.Map.FileSize = (ULONGLONG)fileinfo.st_size;
                pStream->Base.Map.FilePos = 0;
                bResult = true;
            }
        }
        close(handle);
    }

    // Did the mapping fail?
    if(bResult == false)
    {
        nLastError = errno;
        return false;
    }
#endif

    return true;
}

static bool BaseMap_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->Base.Map.FilePos;

    // Do we have to read anything at all?
    if(dwBytesToRead != 0)
    {
        // Don't allow reading past file size
        if((ByteOffset + dwBytesToRead) > pStream->Base.Map.FileSize)
            return false;

        // Copy the required data
        memcpy(pvBuffer, pStream->Base.Map.pbFile + (size_t)ByteOffset, dwBytesToRead);
    }

    // Move the current file position
    pStream->Base.Map.FilePos += dwBytesToRead;
    return true;
}

static void BaseMap_Close(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    if(pStream->Base.Map.pbFile != NULL)
        UnmapViewOfFile(pStream->Base.Map.pbFile);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    if(pStream->Base.Map.pbFile != NULL)
        munmap(pStream->Base.Map.pbFile, (size_t )pStream->Base.Map.FileSize);
#endif

    pStream->Base.Map.pbFile = NULL;
}

// Initializes base functions for the mapped file
static void BaseMap_Init(TFileStream * pStream)
{
    // Supply the file stream functions
    pStream->BaseOpen    = BaseMap_Open;
    pStream->BaseRead    = BaseMap_Read;
    pStream->BaseGetSize = BaseFile_GetSize;    // Reuse BaseFile function
    pStream->BaseGetPos  = BaseFile_GetPos;     // Reuse BaseFile function
    pStream->BaseClose   = BaseMap_Close;

    // Mapped files are read-only
    pStream->dwFlags |= STREAM_FLAG_READ_ONLY;
}

//-----------------------------------------------------------------------------
// Local functions - base HTTP file support
#ifdef PLATFORM_WINDOWS
static const TCHAR * BaseHttp_ExtractServerName(const TCHAR * szFileName, TCHAR * szServerName)
{
    // Check for HTTP
    if(!_tcsnicmp(szFileName, _T("http://"), 7))
        szFileName += 7;

    // Cut off the server name
    if(szServerName != NULL)
    {
        while(szFileName[0] != 0 && szFileName[0] != _T('/'))
            *szServerName++ = *szFileName++;
        *szServerName = 0;
    }
    else
    {
        while(szFileName[0] != 0 && szFileName[0] != _T('/'))
            szFileName++;
    }

    // Return the remainder
    return szFileName;
}
#endif

static bool BaseHttp_Open(TFileStream * pStream, const TCHAR * szFileName, DWORD dwStreamFlags)
{
#ifdef PLATFORM_WINDOWS

    HINTERNET hRequest;
    DWORD dwTemp = 0;
    bool bFileAvailable = false;
    int nError = ERROR_SUCCESS;

    // Keep compiler happy
    dwStreamFlags = dwStreamFlags;

    // Don't connect to the internet
    if(!InternetGetConnectedState(&dwTemp, 0))
        return false;

    // Initiate the connection to the internet
    pStream->Base.Http.hInternet = InternetOpen(_T("StormLib HTTP MPQ reader"),
                                                INTERNET_OPEN_TYPE_PRECONFIG,
                                                NULL,
                                                NULL,
                                                0);
    if(pStream->Base.Http.hInternet == NULL)
        return false;

    // Connect to the server
    if(nError == ERROR_SUCCESS)
    {
        TCHAR szServerName[MAX_PATH];
        DWORD dwFlags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_CACHE_WRITE;

        // Initiate connection with the server
        szFileName = BaseHttp_ExtractServerName(szFileName, szServerName);
        pStream->Base.Http.hConnect = InternetConnect(pStream->Base.Http.hInternet,
                                                      szServerName,
                                                      INTERNET_DEFAULT_HTTP_PORT,
                                                      NULL,
                                                      NULL,
                                                      INTERNET_SERVICE_HTTP,
                                                      dwFlags,
                                                      0);
        if(pStream->Base.Http.hConnect == NULL)
        {
            InternetCloseHandle(pStream->Base.Http.hInternet);
            return false;
        }
    }

    // Now try to query the file size
    if(nError == ERROR_SUCCESS)
    {
        // Open HTTP request to the file
        hRequest = HttpOpenRequest(pStream->Base.Http.hConnect, _T("GET"), szFileName, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if(hRequest != NULL)
        {
            if(HttpSendRequest(hRequest, NULL, 0, NULL, 0))
            {
                ULONGLONG FileTime = 0;
                DWORD dwFileSize = 0;
                DWORD dwDataSize;
                DWORD dwIndex = 0;

                // Check if the MPQ has Last Modified field
                dwDataSize = sizeof(ULONGLONG);
                if(HttpQueryInfo(hRequest, HTTP_QUERY_LAST_MODIFIED | HTTP_QUERY_FLAG_SYSTEMTIME, &FileTime, &dwDataSize, &dwIndex))
                    pStream->Base.Http.FileTime = FileTime;

                // Verify if the server supports random access
                dwDataSize = sizeof(DWORD);
                if(HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &dwFileSize, &dwDataSize, &dwIndex))
                {
                    if(dwFileSize != 0)
                    {
                        pStream->Base.Http.FileSize = dwFileSize;
                        pStream->Base.Http.FilePos = 0;
                        bFileAvailable = true;
                    }
                }
            }
            InternetCloseHandle(hRequest);
        }
    }

    // If the file is not there and is not available for random access,
    // report error
    if(bFileAvailable == false)
    {
        pStream->BaseClose(pStream);
        return false;
    }

    return true;

#else

    // Not supported
    SetLastError(ERROR_NOT_SUPPORTED);
    pStream = pStream;
    return false;

#endif
}

static bool BaseHttp_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
#ifdef PLATFORM_WINDOWS
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->Base.Http.FilePos;
    DWORD dwTotalBytesRead = 0;

    // Do we have to read anything at all?
    if(dwBytesToRead != 0)
    {
        HINTERNET hRequest;
        LPCTSTR szFileName;
        LPBYTE pbBuffer = (LPBYTE)pvBuffer;
        TCHAR szRangeRequest[0x80];
        DWORD dwStartOffset = (DWORD)ByteOffset;
        DWORD dwEndOffset = dwStartOffset + dwBytesToRead;

        // Open HTTP request to the file
        szFileName = BaseHttp_ExtractServerName(pStream->szFileName, NULL);
        hRequest = HttpOpenRequest(pStream->Base.Http.hConnect, _T("GET"), szFileName, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if(hRequest != NULL)
        {
            // Add range request to the HTTP headers
            // http://www.clevercomponents.com/articles/article015/resuming.asp
            _stprintf(szRangeRequest, _T("Range: bytes=%u-%u"), (unsigned int)dwStartOffset, (unsigned int)dwEndOffset);
            HttpAddRequestHeaders(hRequest, szRangeRequest, 0xFFFFFFFF, HTTP_ADDREQ_FLAG_ADD_IF_NEW); 

            // Send the request to the server
            if(HttpSendRequest(hRequest, NULL, 0, NULL, 0))
            {
                while(dwTotalBytesRead < dwBytesToRead)
                {
                    DWORD dwBlockBytesToRead = dwBytesToRead - dwTotalBytesRead;
                    DWORD dwBlockBytesRead = 0;

                    // Read the block from the file
                    if(dwBlockBytesToRead > 0x200)
                        dwBlockBytesToRead = 0x200;
                    InternetReadFile(hRequest, pbBuffer, dwBlockBytesToRead, &dwBlockBytesRead);

                    // Check for end
                    if(dwBlockBytesRead == 0)
                        break;

                    // Move buffers
                    dwTotalBytesRead += dwBlockBytesRead;
                    pbBuffer += dwBlockBytesRead;
                }
            }
            InternetCloseHandle(hRequest);
        }
    }

    // Increment the current file position by number of bytes read
    pStream->Base.Http.FilePos = ByteOffset + dwTotalBytesRead;

    // If the number of bytes read doesn't match the required amount, return false
    if(dwTotalBytesRead != dwBytesToRead)
        SetLastError(ERROR_HANDLE_EOF);
    return (dwTotalBytesRead == dwBytesToRead);

#else

    // Not supported
    pStream = pStream;
    pByteOffset = pByteOffset;
    pvBuffer = pvBuffer;
    dwBytesToRead = dwBytesToRead;
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;

#endif
}

static void BaseHttp_Close(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    if(pStream->Base.Http.hConnect != NULL)
        InternetCloseHandle(pStream->Base.Http.hConnect);
    pStream->Base.Http.hConnect = NULL;

    if(pStream->Base.Http.hInternet != NULL)
        InternetCloseHandle(pStream->Base.Http.hInternet);
    pStream->Base.Http.hInternet = NULL;
#else
    pStream = pStream;
#endif
}

// Initializes base functions for the mapped file
static void BaseHttp_Init(TFileStream * pStream)
{
    // Supply the stream functions
    pStream->BaseOpen    = BaseHttp_Open;
    pStream->BaseRead    = BaseHttp_Read;
    pStream->BaseGetSize = BaseFile_GetSize;    // Reuse BaseFile function
    pStream->BaseGetPos  = BaseFile_GetPos;     // Reuse BaseFile function
    pStream->BaseClose   = BaseHttp_Close;

    // HTTP files are read-only
    pStream->dwFlags |= STREAM_FLAG_READ_ONLY;
}

//-----------------------------------------------------------------------------
// Local functions - base block-based support

// Generic function that loads blocks from the file
// The function groups the block with the same availability,
// so the called BlockRead can finish the request in a single system call
static bool BlockStream_Read(
    TBlockStream * pStream,                 // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG BlockOffset0;
    ULONGLONG BlockOffset;
    ULONGLONG ByteOffset;
    ULONGLONG EndOffset;
    LPBYTE TransferBuffer;
    LPBYTE BlockBuffer;
    DWORD BlockBufferOffset;                // Offset of the desired data in the block buffer
    DWORD BytesNeeded;                      // Number of bytes that really need to be read
    DWORD BlockSize = pStream->BlockSize;
    DWORD BlockCount;
    bool bPrevBlockAvailable;
    bool bCallbackCalled = false;
    bool bBlockAvailable;
    bool bResult = true;

    // The base block read function must be present
    assert(pStream->BlockRead != NULL);

    // NOP reading of zero bytes
    if(dwBytesToRead == 0)
        return true;

    // Get the current position in the stream
    ByteOffset = (pByteOffset != NULL) ? pByteOffset[0] : pStream->StreamPos;
    EndOffset = ByteOffset + dwBytesToRead;
    if(EndOffset > pStream->StreamSize)
    {
        SetLastError(ERROR_HANDLE_EOF);
        return false;
    }

    // Calculate the block parameters
    BlockOffset0 = BlockOffset = ByteOffset & ~((ULONGLONG)BlockSize - 1);
    BlockCount  = (DWORD)(((EndOffset - BlockOffset) + (BlockSize - 1)) / BlockSize);
    BytesNeeded = (DWORD)(EndOffset - BlockOffset);

    // Remember where we have our data
    assert((BlockSize & (BlockSize - 1)) == 0);
    BlockBufferOffset = (DWORD)(ByteOffset & (BlockSize - 1));

    // Allocate buffer for reading blocks
    TransferBuffer = BlockBuffer = STORM_ALLOC(BYTE, (BlockCount * BlockSize));
    if(TransferBuffer == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }

    // If all blocks are available, just read all blocks at once
    if(pStream->IsComplete == 0)
    {
        // Now parse the blocks and send the block read request
        // to all blocks with the same availability
        assert(pStream->BlockCheck != NULL);
        bPrevBlockAvailable = pStream->BlockCheck(pStream, BlockOffset);

        // Loop as long as we have something to read
        while(BlockOffset < EndOffset)
        {
            // Determine availability of the next block
            bBlockAvailable = pStream->BlockCheck(pStream, BlockOffset);

            // If the availability has changed, read all blocks up to this one
            if(bBlockAvailable != bPrevBlockAvailable)
            {
                // Call the file stream callback, if the block is not available
                if(pStream->pMaster && pStream->pfnCallback && bPrevBlockAvailable == false)
                {
                    pStream->pfnCallback(pStream->UserData, BlockOffset0, (DWORD)(BlockOffset - BlockOffset0));
                    bCallbackCalled = true;
                }

                // Load the continuous blocks with the same availability
                assert(BlockOffset > BlockOffset0);
                bResult = pStream->BlockRead(pStream, BlockOffset0, BlockOffset, BlockBuffer, BytesNeeded, bPrevBlockAvailable);
                if(!bResult)
                    break;

                // Move the block offset
                BlockBuffer += (DWORD)(BlockOffset - BlockOffset0);
                BytesNeeded -= (DWORD)(BlockOffset - BlockOffset0);
                bPrevBlockAvailable = bBlockAvailable;
                BlockOffset0 = BlockOffset;
            }

            // Move to the block offset in the stream
            BlockOffset += BlockSize;
        }

        // If there is a block(s) remaining to be read, do it
        if(BlockOffset > BlockOffset0)
        {
            // Call the file stream callback, if the block is not available
            if(pStream->pMaster && pStream->pfnCallback && bPrevBlockAvailable == false)
            {
                pStream->pfnCallback(pStream->UserData, BlockOffset0, (DWORD)(BlockOffset - BlockOffset0));
                bCallbackCalled = true;
            }

            // Read the complete blocks from the file
            if(BlockOffset > pStream->StreamSize)
                BlockOffset = pStream->StreamSize;
            bResult = pStream->BlockRead(pStream, BlockOffset0, BlockOffset, BlockBuffer, BytesNeeded, bPrevBlockAvailable);
        }
    }
    else
    {
        // Read the complete blocks from the file
        if(EndOffset > pStream->StreamSize)
            EndOffset = pStream->StreamSize;
        bResult = pStream->BlockRead(pStream, BlockOffset, EndOffset, BlockBuffer, BytesNeeded, true);
    }

    // Now copy the data to the user buffer
    if(bResult)
    {
        memcpy(pvBuffer, TransferBuffer + BlockBufferOffset, dwBytesToRead);
        pStream->StreamPos = ByteOffset + dwBytesToRead;
    }
    else
    {
        // If the block read failed, set the last error
        SetLastError(ERROR_FILE_INCOMPLETE);
    }

    // Call the callback to indicate we are done
    if(bCallbackCalled)
        pStream->pfnCallback(pStream->UserData, 0, 0);

    // Free the block buffer and return
    STORM_FREE(TransferBuffer);
    return bResult;
}

static bool BlockStream_GetSize(TFileStream * pStream, ULONGLONG * pFileSize)
{
    *pFileSize = pStream->StreamSize;
    return true;
}

static bool BlockStream_GetPos(TFileStream * pStream, ULONGLONG * pByteOffset)
{
    *pByteOffset = pStream->StreamPos;
    return true;
}

static void BlockStream_Close(TBlockStream * pStream)
{
    // Free the data map, if any
    if(pStream->FileBitmap != NULL)
        STORM_FREE(pStream->FileBitmap);
    pStream->FileBitmap = NULL;

    // Call the base class for closing the stream
    pStream->BaseClose(pStream);
}

//-----------------------------------------------------------------------------
// File stream allocation function

static STREAM_INIT StreamBaseInit[4] =
{
    BaseFile_Init,
    BaseMap_Init, 
    BaseHttp_Init,
    BaseNone_Init
};

// This function allocates an empty structure for the file stream
// The stream structure is created as flat block, variable length
// The file name is placed after the end of the stream structure data
static TFileStream * AllocateFileStream(
    const TCHAR * szFileName,
    size_t StreamSize,
    DWORD dwStreamFlags)
{
    TFileStream * pMaster = NULL;
    TFileStream * pStream;
    const TCHAR * szNextFile = szFileName;
    size_t FileNameSize;

    // Sanity check
    assert(StreamSize != 0);

    // The caller can specify chain of files in the following form:
    // C:\archive.MPQ*http://www.server.com/MPQs/archive-server.MPQ
    // In that case, we use the part after "*" as master file name
    while(szNextFile[0] != 0 && szNextFile[0] != _T('*'))
        szNextFile++;
    FileNameSize = (size_t)((szNextFile - szFileName) * sizeof(TCHAR));

    // If we have a next file, we need to open it as master stream
    // Note that we don't care if the master stream exists or not,
    // If it doesn't, later attempts to read missing file block will fail
    if(szNextFile[0] == _T('*'))
    {
        // Don't allow another master file in the string
        if(_tcschr(szNextFile + 1, _T('*')) != NULL)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
        
        // Open the master file
        pMaster = FileStream_OpenFile(szNextFile + 1, STREAM_FLAG_READ_ONLY);
    }

    // Allocate the stream structure for the given stream type
    pStream = (TFileStream *)STORM_ALLOC(BYTE, StreamSize + FileNameSize + sizeof(TCHAR));
    if(pStream != NULL)
    {
        // Zero the entire structure
        memset(pStream, 0, StreamSize);
        pStream->pMaster = pMaster;
        pStream->dwFlags = dwStreamFlags;

        // Initialize the file name
        pStream->szFileName = (TCHAR *)((BYTE *)pStream + StreamSize);
        memcpy(pStream->szFileName, szFileName, FileNameSize);
        pStream->szFileName[FileNameSize / sizeof(TCHAR)] = 0;

        // Initialize the stream functions
        StreamBaseInit[dwStreamFlags & 0x03](pStream);
    }

    return pStream;
}

//-----------------------------------------------------------------------------
// Local functions - flat stream support

static DWORD FlatStream_CheckFile(TBlockStream * pStream)
{
    LPBYTE FileBitmap = (LPBYTE)pStream->FileBitmap;
    DWORD WholeByteCount = (pStream->BlockCount / 8);
    DWORD ExtraBitsCount = (pStream->BlockCount & 7);
    BYTE ExpectedValue;

    // Verify the whole bytes - their value must be 0xFF
    for(DWORD i = 0; i < WholeByteCount; i++)
    {
        if(FileBitmap[i] != 0xFF)
            return 0;
    }

    // If there are extra bits, calculate the mask
    if(ExtraBitsCount != 0)
    {
        ExpectedValue = (BYTE)((1 << ExtraBitsCount) - 1);
        if(FileBitmap[WholeByteCount] != ExpectedValue)
            return 0;
    }

    // Yes, the file is complete
    return 1;
}

static bool FlatStream_LoadBitmap(TBlockStream * pStream)
{
    FILE_BITMAP_FOOTER Footer;
    ULONGLONG ByteOffset; 
    LPBYTE FileBitmap;
    DWORD BlockCount;
    DWORD BitmapSize;

    // Do not load the bitmap if we should not have to
    if(!(pStream->dwFlags & STREAM_FLAG_USE_BITMAP))
        return false;

    // Only if the size is greater than size of bitmap footer
    if(pStream->Base.File.FileSize > sizeof(FILE_BITMAP_FOOTER))
    {
        // Load the bitmap footer
        ByteOffset = pStream->Base.File.FileSize - sizeof(FILE_BITMAP_FOOTER);
        if(pStream->BaseRead(pStream, &ByteOffset, &Footer, sizeof(FILE_BITMAP_FOOTER)))
        {
            // Make sure that the array is properly BSWAP-ed
            BSWAP_ARRAY32_UNSIGNED((LPDWORD)(&Footer), sizeof(FILE_BITMAP_FOOTER));

            // Verify if there is actually a footer
            if(Footer.Signature == ID_FILE_BITMAP_FOOTER && Footer.Version == 0x03)
            {
                // Get the offset of the bitmap, number of blocks and size of the bitmap
                ByteOffset = MAKE_OFFSET64(Footer.MapOffsetHi, Footer.MapOffsetLo);
                BlockCount = (DWORD)(((ByteOffset - 1) / Footer.BlockSize) + 1);
                BitmapSize = ((BlockCount + 7) / 8);

                // Check if the sizes match
                if(ByteOffset + BitmapSize + sizeof(FILE_BITMAP_FOOTER) == pStream->Base.File.FileSize)
                {
                    // Allocate space for the bitmap
                    FileBitmap = STORM_ALLOC(BYTE, BitmapSize);
                    if(FileBitmap != NULL)
                    {
                        // Load the bitmap bits
                        if(!pStream->BaseRead(pStream, &ByteOffset, FileBitmap, BitmapSize))
                        {
                            STORM_FREE(FileBitmap);
                            return false;
                        }

                        // Update the stream size
                        pStream->BuildNumber = Footer.BuildNumber;
                        pStream->StreamSize = ByteOffset;

                        // Fill the bitmap information
                        pStream->FileBitmap = FileBitmap;
                        pStream->BitmapSize = BitmapSize;
                        pStream->BlockSize  = Footer.BlockSize;
                        pStream->BlockCount = BlockCount;
                        pStream->IsComplete = FlatStream_CheckFile(pStream);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static void FlatStream_UpdateBitmap(
    TBlockStream * pStream,                // Pointer to an open stream
    ULONGLONG StartOffset,
    ULONGLONG EndOffset)
{
    LPBYTE FileBitmap = (LPBYTE)pStream->FileBitmap;
    DWORD BlockIndex;
    DWORD BlockSize = pStream->BlockSize;
    DWORD ByteIndex;
    BYTE BitMask;

    // Sanity checks
    assert((StartOffset & (BlockSize - 1)) == 0);
    assert(FileBitmap != NULL);

    // Calculate the index of the block
    BlockIndex = (DWORD)(StartOffset / BlockSize);
    ByteIndex = (BlockIndex / 0x08);
    BitMask = (BYTE)(1 << (BlockIndex & 0x07));

    // Set all bits for the specified range
    while(StartOffset < EndOffset)
    {
        // Set the bit
        FileBitmap[ByteIndex] |= BitMask;

        // Move all
        StartOffset += BlockSize;
        ByteIndex += (BitMask >> 0x07);
        BitMask = (BitMask >> 0x07) | (BitMask << 0x01);
    }

    // Increment the bitmap update count
    pStream->IsModified = 1;
}

static bool FlatStream_BlockCheck(
    TBlockStream * pStream,                // Pointer to an open stream
    ULONGLONG BlockOffset)
{
    LPBYTE FileBitmap = (LPBYTE)pStream->FileBitmap;
    DWORD BlockIndex;
    BYTE BitMask;

    // Sanity checks
    assert((BlockOffset & (pStream->BlockSize - 1)) == 0);
    assert(FileBitmap != NULL);
    
    // Calculate the index of the block
    BlockIndex = (DWORD)(BlockOffset / pStream->BlockSize);
    BitMask = (BYTE)(1 << (BlockIndex & 0x07));

    // Check if the bit is present
    return (FileBitmap[BlockIndex / 0x08] & BitMask) ? true : false;
}

static bool FlatStream_BlockRead(
    TBlockStream * pStream,                // Pointer to an open stream
    ULONGLONG StartOffset,
    ULONGLONG EndOffset,
    LPBYTE BlockBuffer,
    DWORD BytesNeeded,
    bool bAvailable)
{
    DWORD BytesToRead = (DWORD)(EndOffset - StartOffset);

    // The starting offset must be aligned to size of the block
    assert(pStream->FileBitmap != NULL);
    assert((StartOffset & (pStream->BlockSize - 1)) == 0);
    assert(StartOffset < EndOffset);

    // If the blocks are not available, we need to load them from the master
    // and then save to the mirror
    if(bAvailable == false)
    {
        // If we have no master, we cannot satisfy read request
        if(pStream->pMaster == NULL)
            return false;

        // Load the blocks from the master stream
        // Note that we always have to read complete blocks
        // so they get properly stored to the mirror stream
        if(!FileStream_Read(pStream->pMaster, &StartOffset, BlockBuffer, BytesToRead))
            return false;

        // Store the loaded blocks to the mirror file.
        // Note that this operation is not required to succeed
        if(pStream->BaseWrite(pStream, &StartOffset, BlockBuffer, BytesToRead))
            FlatStream_UpdateBitmap(pStream, StartOffset, EndOffset);

        return true;
    }
    else
    {
        if(BytesToRead > BytesNeeded)
            BytesToRead = BytesNeeded;
        return pStream->BaseRead(pStream, &StartOffset, BlockBuffer, BytesToRead);
    }
}

static void FlatStream_Close(TBlockStream * pStream)
{
    FILE_BITMAP_FOOTER Footer;

    if(pStream->FileBitmap && pStream->IsModified)
    {
        // Write the file bitmap
        pStream->BaseWrite(pStream, &pStream->StreamSize, pStream->FileBitmap, pStream->BitmapSize);
        
        // Prepare and write the file footer
        Footer.Signature   = ID_FILE_BITMAP_FOOTER;
        Footer.Version     = 3;
        Footer.BuildNumber = pStream->BuildNumber;
        Footer.MapOffsetLo = (DWORD)(pStream->StreamSize & 0xFFFFFFFF);
        Footer.MapOffsetHi = (DWORD)(pStream->StreamSize >> 0x20);
        Footer.BlockSize   = pStream->BlockSize;
        BSWAP_ARRAY32_UNSIGNED(&Footer, sizeof(FILE_BITMAP_FOOTER));
        pStream->BaseWrite(pStream, NULL, &Footer, sizeof(FILE_BITMAP_FOOTER));
    }

    // Close the base class
    BlockStream_Close(pStream);
}

static bool FlatStream_CreateMirror(TBlockStream * pStream)
{
    ULONGLONG MasterSize = 0;
    ULONGLONG MirrorSize = 0;
    LPBYTE FileBitmap = NULL;
    DWORD dwBitmapSize;
    DWORD dwBlockCount;
    bool bNeedCreateMirrorStream = true;
    bool bNeedResizeMirrorStream = true;

    // Do we have master function and base creation function?
    if(pStream->pMaster == NULL || pStream->BaseCreate == NULL)
        return false;

    // Retrieve the master file size, block count and bitmap size
    FileStream_GetSize(pStream->pMaster, &MasterSize);
    dwBlockCount = (DWORD)((MasterSize + DEFAULT_BLOCK_SIZE - 1) / DEFAULT_BLOCK_SIZE);
    dwBitmapSize = (DWORD)((dwBlockCount + 7) / 8);

    // Setup stream size and position
    pStream->BuildNumber = DEFAULT_BUILD_NUMBER;        // BUGBUG: Really???
    pStream->StreamSize = MasterSize;
    pStream->StreamPos = 0;

    // Open the base stream for write access
    if(pStream->BaseOpen(pStream, pStream->szFileName, 0))
    {
        // If the file open succeeded, check if the file size matches required size
        pStream->BaseGetSize(pStream, &MirrorSize);
        if(MirrorSize == MasterSize + dwBitmapSize + sizeof(FILE_BITMAP_FOOTER))
        {
            // Attempt to load an existing file bitmap
            if(FlatStream_LoadBitmap(pStream))
                return true;

            // We need to create new file bitmap
            bNeedResizeMirrorStream = false;
        }

        // We need to create mirror stream
        bNeedCreateMirrorStream = false;
    }

    // Create a new stream, if needed
    if(bNeedCreateMirrorStream)
    {
        if(!pStream->BaseCreate(pStream))
            return false;
    }

    // If we need to, then resize the mirror stream
    if(bNeedResizeMirrorStream)
    {
        if(!pStream->BaseResize(pStream, MasterSize + dwBitmapSize + sizeof(FILE_BITMAP_FOOTER)))
            return false;
    }

    // Allocate the bitmap array
    FileBitmap = STORM_ALLOC(BYTE, dwBitmapSize);
    if(FileBitmap == NULL)
        return false;

    // Initialize the bitmap
    memset(FileBitmap, 0, dwBitmapSize);
    pStream->FileBitmap = FileBitmap;
    pStream->BitmapSize = dwBitmapSize;
    pStream->BlockSize  = DEFAULT_BLOCK_SIZE;
    pStream->BlockCount = dwBlockCount;
    pStream->IsComplete = 0;
    pStream->IsModified = 1;

    // Note: Don't write the stream bitmap right away.
    // Doing so would cause sparse file resize on NTFS,
    // which would take long time on larger files.
    return true;
}

static TFileStream * FlatStream_Open(const TCHAR * szFileName, DWORD dwStreamFlags)
{
    TBlockStream * pStream;    
    ULONGLONG ByteOffset = 0;

    // Create new empty stream
    pStream = (TBlockStream *)AllocateFileStream(szFileName, sizeof(TBlockStream), dwStreamFlags);
    if(pStream == NULL)
        return NULL;

    // Do we have a master stream?
    if(pStream->pMaster != NULL)
    {
        if(!FlatStream_CreateMirror(pStream))
        {
            FileStream_Close(pStream);
            SetLastError(ERROR_FILE_NOT_FOUND);
            return NULL;
        }
    }
    else
    {
        // Attempt to open the base stream
        if(!pStream->BaseOpen(pStream, pStream->szFileName, dwStreamFlags))
        {
            FileStream_Close(pStream);
            return NULL;
        }

        // Load the bitmap, if required to
        if(dwStreamFlags & STREAM_FLAG_USE_BITMAP)
            FlatStream_LoadBitmap(pStream);
    }

    // If we have a stream bitmap, set the reading functions
    // which check presence of each file block
    if(pStream->FileBitmap != NULL)
    {
        // Set the stream position to zero. Stream size is already set
        assert(pStream->StreamSize != 0);
        pStream->StreamPos = 0;
        pStream->dwFlags |= STREAM_FLAG_READ_ONLY;

        // Supply the stream functions
        pStream->StreamRead    = (STREAM_READ)BlockStream_Read;
        pStream->StreamGetSize = BlockStream_GetSize;
        pStream->StreamGetPos  = BlockStream_GetPos;
        pStream->StreamClose   = (STREAM_CLOSE)FlatStream_Close;

        // Supply the block functions
        pStream->BlockCheck    = (BLOCK_CHECK)FlatStream_BlockCheck;
        pStream->BlockRead     = (BLOCK_READ)FlatStream_BlockRead;
    }
    else
    {
        // Reset the base position to zero
        pStream->BaseRead(pStream, &ByteOffset, NULL, 0);

        // Setup stream size and position
        pStream->StreamSize = pStream->Base.File.FileSize;
        pStream->StreamPos = 0;

        // Set the base functions
        pStream->StreamRead    = pStream->BaseRead;
        pStream->StreamWrite   = pStream->BaseWrite;
        pStream->StreamResize  = pStream->BaseResize;
        pStream->StreamGetSize = pStream->BaseGetSize;
        pStream->StreamGetPos  = pStream->BaseGetPos;
        pStream->StreamClose   = pStream->BaseClose;
    }

    return pStream;
}

//-----------------------------------------------------------------------------
// Public functions

/**
 * This function creates a new file for read-write access
 *
 * - If the current platform supports file sharing,
 *   the file must be created for read sharing (i.e. another application
 *   can open the file for read, but not for write)
 * - If the file does not exist, the function must create new one
 * - If the file exists, the function must rewrite it and set to zero size
 * - The parameters of the function must be validate by the caller
 * - The function must initialize all stream function pointers in TFileStream
 * - If the function fails from any reason, it must close all handles
 *   and free all memory that has been allocated in the process of stream creation,
 *   including the TFileStream structure itself
 *
 * \a szFileName Name of the file to create
 */

TFileStream * FileStream_CreateFile(
    const TCHAR * szFileName,
    DWORD dwStreamFlags)
{
    TFileStream * pStream;

    // We only support creation of flat, local file
    if((dwStreamFlags & (STREAM_PROVIDERS_MASK)) != (STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE))
    {
        SetLastError(ERROR_NOT_SUPPORTED);
        return NULL;
    }

    // Allocate file stream structure for flat stream
    pStream = AllocateFileStream(szFileName, sizeof(TBlockStream), dwStreamFlags);
    if(pStream != NULL)
    {
        // Attempt to create the disk file
        if(BaseFile_Create(pStream))
        {
            // Fill the stream provider functions
            pStream->StreamRead    = pStream->BaseRead;
            pStream->StreamWrite   = pStream->BaseWrite;
            pStream->StreamResize  = pStream->BaseResize;
            pStream->StreamGetSize = pStream->BaseGetSize;
            pStream->StreamGetPos  = pStream->BaseGetPos;
            pStream->StreamClose   = pStream->BaseClose;
            return pStream;
        }

        // File create failed, delete the stream
        STORM_FREE(pStream);
        pStream = NULL;
    }

    // Return the stream
    return pStream;
}

/**
 * This function opens an existing file for read or read-write access
 * - If the current platform supports file sharing,
 *   the file must be open for read sharing (i.e. another application
 *   can open the file for read, but not for write)
 * - If the file does not exist, the function must return NULL
 * - If the file exists but cannot be open, then function must return NULL
 * - The parameters of the function must be validate by the caller
 * - The function must initialize all stream function pointers in TFileStream
 * - If the function fails from any reason, it must close all handles
 *   and free all memory that has been allocated in the process of stream creation,
 *   including the TFileStream structure itself
 *
 * \a szFileName Name of the file to open
 * \a dwStreamFlags specifies the provider and base storage type
 */

TFileStream * FileStream_OpenFile(
    const TCHAR * szFileName,
    DWORD dwStreamFlags)
{
    DWORD dwProvider = dwStreamFlags & STREAM_PROVIDERS_MASK;
    size_t nPrefixLength = FileStream_Prefix(szFileName, &dwProvider);

    // Re-assemble the stream flags
    dwStreamFlags = (dwStreamFlags & STREAM_OPTIONS_MASK) | dwProvider;
    szFileName += nPrefixLength;

    // Perform provider-specific open
    switch(dwStreamFlags & STREAM_PROVIDER_MASK)
    {
        case STREAM_PROVIDER_FLAT:
            return FlatStream_Open(szFileName, dwStreamFlags);

        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
    }
}

/**
 * Returns the file name of the stream
 *
 * \a pStream Pointer to an open stream
 */
const TCHAR * FileStream_GetFileName(TFileStream * pStream)
{
    assert(pStream != NULL);
    return pStream->szFileName;
}

/**
 * Returns the length of the provider prefix. Returns zero if no prefix
 *
 * \a szFileName Pointer to a stream name (file, mapped file, URL)
 * \a pdwStreamProvider Pointer to a DWORD variable that receives stream provider (STREAM_PROVIDER_XXX)
 */

size_t FileStream_Prefix(const TCHAR * szFileName, DWORD * pdwProvider)
{
    size_t nPrefixLength1 = 0;
    size_t nPrefixLength2 = 0;
    DWORD dwProvider = 0;

    if(szFileName != NULL)
    {
        //
        // Determine the stream provider
        //

        if(!_tcsnicmp(szFileName, _T("flat-"), 5))
        {
            dwProvider |= STREAM_PROVIDER_FLAT;
            nPrefixLength1 = 5;
        }

        else if(!_tcsnicmp(szFileName, _T("part-"), 5))
        {
            dwProvider |= STREAM_PROVIDER_PARTIAL;
            nPrefixLength1 = 5;
        }

        else if(!_tcsnicmp(szFileName, _T("mpqe-"), 5))
        {
            dwProvider |= STREAM_PROVIDER_MPQE;
            nPrefixLength1 = 5;
        }

        else if(!_tcsnicmp(szFileName, _T("blk4-"), 5))
        {
            dwProvider |= STREAM_PROVIDER_BLOCK4;
            nPrefixLength1 = 5;
        }

        //
        // Determine the base provider
        //

        if(!_tcsnicmp(szFileName+nPrefixLength1, _T("file:"), 5))
        {
            dwProvider |= BASE_PROVIDER_FILE;
            nPrefixLength2 = 5;
        }

        // Only accept stream provider if we recognized the base provider
        if(nPrefixLength2 != 0)
        {
            // It is also allowed to put "//" after the base provider, e.g. "file://", "http://"
            if(szFileName[nPrefixLength1+nPrefixLength2] == '/' && szFileName[nPrefixLength1+nPrefixLength2+1] == '/')
                nPrefixLength2 += 2;

            if(pdwProvider != NULL)
                *pdwProvider = dwProvider;
            return nPrefixLength1 + nPrefixLength2;
        }
    }

    return 0;
}

/**
 * Reads data from the stream
 *
 * - Returns true if the read operation succeeded and all bytes have been read
 * - Returns false if either read failed or not all bytes have been read
 * - If the pByteOffset is NULL, the function must read the data from the current file position
 * - The function can be called with dwBytesToRead = 0. In that case, pvBuffer is ignored
 *   and the function just adjusts file pointer.
 *
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, it reads from the current position
 * \a pvBuffer Pointer to data to be read
 * \a dwBytesToRead Number of bytes to read from the file
 *
 * \returns
 * - If the function reads the required amount of bytes, it returns true.
 * - If the function reads less than required bytes, it returns false and GetLastError() returns ERROR_HANDLE_EOF
 * - If the function fails, it reads false and GetLastError() returns an error code different from ERROR_HANDLE_EOF
 */
bool FileStream_Read(TFileStream * pStream, ULONGLONG * pByteOffset, void * pvBuffer, DWORD dwBytesToRead)
{
    assert(pStream->StreamRead != NULL);
    return pStream->StreamRead(pStream, pByteOffset, pvBuffer, dwBytesToRead);
}

/**
 * This function writes data to the stream
 *
 * - Returns true if the write operation succeeded and all bytes have been written
 * - Returns false if either write failed or not all bytes have been written
 * - If the pByteOffset is NULL, the function must write the data to the current file position
 *
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, it reads from the current position
 * \a pvBuffer Pointer to data to be written
 * \a dwBytesToWrite Number of bytes to write to the file
 */
bool FileStream_Write(TFileStream * pStream, ULONGLONG * pByteOffset, const void * pvBuffer, DWORD dwBytesToWrite)
{
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return false;
    }

    assert(pStream->StreamWrite != NULL);
    return pStream->StreamWrite(pStream, pByteOffset, pvBuffer, dwBytesToWrite);
}

/**
 * Returns the size of a file
 *
 * \a pStream Pointer to an open stream
 * \a FileSize Pointer where to store the file size
 */
bool FileStream_GetSize(TFileStream * pStream, ULONGLONG * pFileSize)
{
    assert(pStream->StreamGetSize != NULL);
    return pStream->StreamGetSize(pStream, pFileSize);
}

/**
 * Sets the size of a file
 *
 * \a pStream Pointer to an open stream
 * \a NewFileSize File size to set
 */
bool FileStream_SetSize(TFileStream * pStream, ULONGLONG NewFileSize)
{                                 
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return false;
    }

    assert(pStream->StreamResize != NULL);
    return pStream->StreamResize(pStream, NewFileSize);
}

/**
 * This function returns the current file position
 * \a pStream
 * \a pByteOffset
 */
bool FileStream_GetPos(TFileStream * pStream, ULONGLONG * pByteOffset)
{
    assert(pStream->StreamGetPos != NULL);
    return pStream->StreamGetPos(pStream, pByteOffset);
}

/**
 * Returns the last write time of a file
 *
 * \a pStream Pointer to an open stream
 * \a pFileType Pointer where to store the file last write time
 */
bool FileStream_GetTime(TFileStream * pStream, ULONGLONG * pFileTime)
{
    // Just use the saved filetime value
    *pFileTime = pStream->Base.File.FileTime;
    return true;
}

/**
 * Returns the stream flags
 *
 * \a pStream Pointer to an open stream
 * \a pdwStreamFlags Pointer where to store the stream flags
 */
bool FileStream_GetFlags(TFileStream * pStream, LPDWORD pdwStreamFlags)
{
    *pdwStreamFlags = pStream->dwFlags;
    return true;
}

/**
 * Switches a stream with another. Used for final phase of archive compacting.
 * Performs these steps:
 *
 * 1) Closes the handle to the existing MPQ
 * 2) Renames the temporary MPQ to the original MPQ, overwrites existing one
 * 3) Opens the MPQ stores the handle and stream position to the new stream structure
 *
 * \a pStream Pointer to an open stream
 * \a pNewStream Temporary ("working") stream (created during archive compacting)
 */
bool FileStream_Replace(TFileStream * pStream, TFileStream * pNewStream)
{
    // Only supported on flat files
    if((pStream->dwFlags & STREAM_PROVIDERS_MASK) != (STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE))
    {
        SetLastError(ERROR_NOT_SUPPORTED);
        return false;
    }

    // Not supported on read-only streams
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return false;
    }

    // Close both stream's base providers
    pNewStream->BaseClose(pNewStream);
    pStream->BaseClose(pStream);

    // Now we have to delete the (now closed) old file and rename the new file
    if(!BaseFile_Replace(pStream, pNewStream))
        return false;

    // Now open the base file again
    if(!BaseFile_Open(pStream, pStream->szFileName, pStream->dwFlags))
        return false;

    // Cleanup the new stream
    FileStream_Close(pNewStream);
    return true;
}

/**
 * This function closes an archive file and frees any data buffers
 * that have been allocated for stream management. The function must also
 * support partially allocated structure, i.e. one or more buffers
 * can be NULL, if there was an allocation failure during the process
 *
 * \a pStream Pointer to an open stream
 */
void FileStream_Close(TFileStream * pStream)
{
    // Check if the stream structure is allocated at all
    if(pStream != NULL)
    {
        // Free the master stream, if any
        if(pStream->pMaster != NULL)
            FileStream_Close(pStream->pMaster);
        pStream->pMaster = NULL;

        // Close the stream provider ...
        if(pStream->StreamClose != NULL)
            pStream->StreamClose(pStream);
        
        // ... or close base stream, if any
        else if(pStream->BaseClose != NULL)
            pStream->BaseClose(pStream);

        // Free the stream itself
        STORM_FREE(pStream);
    }
}

//-----------------------------------------------------------------------------
// Utility functions (ANSI)

const char * GetPlainFileName(const char * szFileName)
{
    const char * szPlainName = szFileName;

    while(*szFileName != 0)
    {
        if(*szFileName == '\\' || *szFileName == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    return szPlainName;
}

void CopyFileName(char * szTarget, const char * szSource, size_t cchLength)
{
    memcpy(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}

//-----------------------------------------------------------------------------
// Utility functions (UNICODE) only exist in the ANSI version of the library
// In ANSI builds, TCHAR = char, so we don't need these functions implemented

#ifdef _UNICODE
const TCHAR * GetPlainFileName(const TCHAR * szFileName)
{
    const TCHAR * szPlainName = szFileName;

    while(*szFileName != 0)
    {
        if(*szFileName == '\\' || *szFileName == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    return szPlainName;
}

void CopyFileName(TCHAR * szTarget, const char * szSource, size_t cchLength)
{
    mbstowcs(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}

void CopyFileName(char * szTarget, const TCHAR * szSource, size_t cchLength)
{
    wcstombs(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}
#endif
