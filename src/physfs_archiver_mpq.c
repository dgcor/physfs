/*
 * MPQ support routines for PhysicsFS.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_MPQ

#if PHYSFS_USE_EXTERNAL_STORMLIB
#include "../StormLib/src/StormLib.h"
#else
#include "StormLib/StormLib.h"
#endif

typedef struct
{
    PHYSFS_Io *io;
    HANDLE mpqHandle;
} MPQHandle;

typedef struct
{
    HANDLE fileHandle;
    PHYSFS_sint64 size;
} MPQFileHandle;

static PHYSFS_sint64 MPQ_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 len)
{
    MPQFileHandle *handle = (MPQFileHandle*)io->opaque;
    DWORD dwBytesRead;
    DWORD dwBytesToRead;

    if (len < (PHYSFS_uint64)handle->size)
        dwBytesToRead = (DWORD)len;
    else
        dwBytesToRead = (DWORD)handle->size;

    SFileReadFile(handle->fileHandle, buf, dwBytesToRead, &dwBytesRead, NULL);
    if (dwBytesRead != dwBytesToRead)
        return -1L;

    return (PHYSFS_sint64)dwBytesRead;
}

static PHYSFS_sint64 MPQ_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
}

static PHYSFS_sint64 MPQ_tell(PHYSFS_Io *io)
{
    MPQFileHandle *handle = (MPQFileHandle*)io->opaque;
    LONG FilePosHi = 0;
    DWORD FilePosLo;
    FilePosLo = SFileSetFilePointer(handle->fileHandle, 0, &FilePosHi, FILE_CURRENT);
    return (((PHYSFS_sint64)FilePosHi << 32) | (PHYSFS_sint64)FilePosLo);
}

static int MPQ_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    MPQFileHandle *handle = (MPQFileHandle*)io->opaque;
    LONG DeltaPosHi = (LONG)(offset >> 32);
    LONG DeltaPosLo = (LONG)(offset);
    SFileSetFilePointer(handle->fileHandle, DeltaPosLo, &DeltaPosHi, FILE_BEGIN);
    return 1;
}

static PHYSFS_sint64 MPQ_length(PHYSFS_Io *io)
{
    MPQFileHandle *handle = (MPQFileHandle*)io->opaque;

    if (handle->fileHandle)
        return handle->size;

    return -1L;
}

static PHYSFS_Io *MPQ_duplicate(PHYSFS_Io *io)
{
    BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);  /* !!! FIXME: write me. */
}

static int MPQ_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void MPQ_destroy(PHYSFS_Io *io)
{
    MPQFileHandle *handle = (MPQFileHandle*)io->opaque;
    if (handle != NULL)
    {
        if (handle->fileHandle != NULL)
        {
            SFileCloseFile(handle->fileHandle);
        }
        physfs_alloc.Free(handle);
    }
    physfs_alloc.Free(io);
}

static const PHYSFS_Io MPQ_Io =
{
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    MPQ_read,
    MPQ_write,
    MPQ_seek,
    MPQ_tell,
    MPQ_length,
    MPQ_duplicate,
    MPQ_flush,
    MPQ_destroy
};

static void *MPQ_openArchive(PHYSFS_Io *io, const char *name,
                             int forWriting, int *claimed)
{
    HANDLE hMpq = NULL;
    DWORD dwFlags = MPQ_OPEN_READ_ONLY;
    MPQHandle *handle = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    if (!SFileOpenArchive(name, 0, dwFlags, &hMpq))
        return NULL;

    *claimed = 1;

    handle = (MPQHandle *)physfs_alloc.Malloc(sizeof(MPQHandle));
    if (handle)
    {
        handle->io = io;
        handle->mpqHandle = hMpq;
    }
    return handle;
}

static PHYSFS_EnumerateCallbackResult MPQ_enumerate(void *opaque,
                         const char *dname, PHYSFS_EnumerateCallback cb,
                         const char *origdir, void *callbackdata)
{
    return PHYSFS_ENUM_ERROR;
}

static char *MPQ_getValidFilename(const char *filename)
{
    char *filename2 = NULL;
    char *chr;

    filename2 = __PHYSFS_strdup(filename);
    if (!filename2)
        return NULL;

    chr = filename2;
    while (chr[0] != 0)
    {
        if (chr[0] == '/')
        {
            chr[0] = '\\';
        }
        chr++;
    }
    return filename2;
}

static PHYSFS_Io *MPQ_openRead(void *opaque, const char *filename)
{
    char *filename2 = NULL;
    HANDLE hFile;
    PHYSFS_Io *retval = NULL;
    MPQFileHandle *handle = NULL;
    DWORD dwFileSizeHi = 0xCCCCCCCC;
    DWORD dwFileSizeLo = 0;
    char success;

    if (!opaque)
        return NULL;

    filename2 = MPQ_getValidFilename(filename);
    if (!filename2)
        return NULL;

    success = SFileOpenFileEx(((MPQHandle *)opaque)->mpqHandle, filename2, 0, &hFile);
    physfs_alloc.Free(filename2);

    if (!success)
        return NULL;

    retval = (PHYSFS_Io *)physfs_alloc.Malloc(sizeof(PHYSFS_Io));
    if (!retval)
    {
        SFileCloseFile(hFile);
        return NULL;
    }

    handle = (MPQFileHandle *)physfs_alloc.Malloc(sizeof(MPQFileHandle));
    if (!handle)
    {
        physfs_alloc.Free(retval);
        SFileCloseFile(hFile);
        return NULL;
    }

    dwFileSizeLo = SFileGetFileSize(hFile, &dwFileSizeHi);
    if (dwFileSizeLo == SFILE_INVALID_SIZE || dwFileSizeHi != 0)
    {
        physfs_alloc.Free(retval);
        physfs_alloc.Free(hFile);
        SFileCloseFile(hFile);
        return NULL;
    }

    handle->fileHandle = hFile;
    handle->size = (PHYSFS_sint64)dwFileSizeLo;

    memcpy(retval, &MPQ_Io, sizeof(PHYSFS_Io));
    retval->opaque = handle;

    return retval;
}

static PHYSFS_Io *MPQ_openWrite(void *opaque, const char *filename)
{
    BAIL(PHYSFS_ERR_READ_ONLY, NULL);
}

static PHYSFS_Io *MPQ_openAppend(void *opaque, const char *filename)
{
    BAIL(PHYSFS_ERR_READ_ONLY, NULL);
}

static int MPQ_remove(void *opaque, const char *name)
{
    BAIL(PHYSFS_ERR_READ_ONLY, 0);
}

static int MPQ_mkdir(void *opaque, const char *name)
{
    BAIL(PHYSFS_ERR_READ_ONLY, 0);
}

static int MPQ_stat(void *opaque, const char *filename, PHYSFS_Stat *stat)
{
    char *filename2 = NULL;
    HANDLE hFile;
    char success;
    DWORD fileSize = 0;

    if (!opaque)
        return 0;

    filename2 = MPQ_getValidFilename(filename);
    if (!filename2)
        return 0;

    success = SFileOpenFileEx(((MPQHandle *)opaque)->mpqHandle, filename2, 0, &hFile);
    physfs_alloc.Free(filename2);

    if (!success)
        return 0;

    SFileGetFileInfo(hFile, SFileInfoFileSize, &fileSize, sizeof(fileSize), NULL);
    stat->filesize = fileSize;

    stat->modtime = 0;
    SFileGetFileInfo(hFile, SFileInfoFileTime, &stat->modtime, sizeof(stat->modtime), NULL);
    stat->createtime = stat->modtime;
    stat->accesstime = 0;
    stat->filetype = PHYSFS_FILETYPE_REGULAR;
    stat->readonly = 1; /* .MPQ files are always read only */

    SFileCloseFile(hFile);

    return 1;
}

static void MPQ_closeArchive(void *opaque)
{
    MPQHandle *handle = (MPQHandle*)opaque;

    if (!handle)
        return;

    SFileCloseArchive(handle->mpqHandle);
    handle->io->destroy(handle->io);
    physfs_alloc.Free(handle);
}

const PHYSFS_Archiver __PHYSFS_Archiver_MPQ =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "MPQ",
        "Blizzard Entertainment format",
        "",
        "",
        1,  /* supportsSymlinks */
    },
    MPQ_openArchive,
    MPQ_enumerate,
    MPQ_openRead,
    MPQ_openWrite,
    MPQ_openAppend,
    MPQ_remove,
    MPQ_mkdir,
    MPQ_stat,
    MPQ_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_MPQ */

/* end of archiver_mpq.c ... */

