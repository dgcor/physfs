/*
 * Posix-esque support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/* !!! FIXME: check for EINTR? */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_POSIX

#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#if ((!defined PHYSFS_NO_THREAD_SUPPORT) && (!defined PHYSFS_PLATFORM_BEOS))
#include <pthread.h>
#endif

#include "physfs_internal.h"


static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
        case EIO: return PHYSFS_ERR_IO;
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
        case EMLINK: return PHYSFS_ERR_NO_SPACE;
        case ENAMETOOLONG: return PHYSFS_ERR_BAD_FILENAME;
        case ENOENT: return PHYSFS_ERR_NOT_FOUND;
        case ENOSPC: return PHYSFS_ERR_NO_SPACE;
        case ENOTDIR: return PHYSFS_ERR_NOT_FOUND;
        case EISDIR: return PHYSFS_ERR_NOT_A_FILE;
        case EROFS: return PHYSFS_ERR_READ_ONLY;
        case ETXTBSY: return PHYSFS_ERR_BUSY;
        case EBUSY: return PHYSFS_ERR_BUSY;
        case ENOMEM: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ENOTEMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromErrnoError */


static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */


static char *getUserDirByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_dir != NULL) && (*pw->pw_dir != '\0'))
    {
        const size_t dlen = strlen(pw->pw_dir);
        const size_t add_dirsep = (pw->pw_dir[dlen-1] != '/') ? 1 : 0;
        retval = (char *) physfs_alloc.Malloc(dlen + 1 + add_dirsep);
        if (retval != NULL)
        {
            strcpy(retval, pw->pw_dir);
            if (add_dirsep)
            {
                retval[dlen] = '/';
                retval[dlen+1] = '\0';
            } /* if */
        } /* if */
    } /* if */
    
    return retval;
} /* getUserDirByUID */


char *__PHYSFS_platformCalcUserDir(void)
{
    char *retval = NULL;
    char *envr = getenv("HOME");

    /* if the environment variable was set, make sure it's really a dir. */
    if (envr != NULL)
    {
        struct stat statbuf;
        if ((stat(envr, &statbuf) != -1) && (S_ISDIR(statbuf.st_mode)))
        {
            const size_t envrlen = strlen(envr);
            const size_t add_dirsep = (envr[envrlen-1] != '/') ? 1 : 0;
            retval = physfs_alloc.Malloc(envrlen + 1 + add_dirsep);
            if (retval)
            {
                strcpy(retval, envr);
                if (add_dirsep)
                {
                    retval[envrlen] = '/';
                    retval[envrlen+1] = '\0';
                } /* if */
            } /* if */
        } /* if */
    } /* if */

    if (retval == NULL)
        retval = getUserDirByUID();

    return retval;
} /* __PHYSFS_platformCalcUserDir */


void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     PHYSFS_EnumFilesCallback callback,
                                     const char *origdir,
                                     void *callbackdata)
{
    DIR *dir;
    struct dirent *ent;
    char *buf = NULL;

    errno = 0;
    dir = opendir(dirname);
    if (dir == NULL)
    {
        physfs_alloc.Free(buf);
        return;
    } /* if */

    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0)
            continue;
        else if (strcmp(ent->d_name, "..") == 0)
            continue;

        callback(callbackdata, origdir, ent->d_name);
    } /* while */

    physfs_alloc.Free(buf);
    closedir(dir);
} /* __PHYSFS_platformEnumerateFiles */


int __PHYSFS_platformMkDir(const char *path)
{
    const int rc = mkdir(path, S_IRWXU);
    BAIL_IF_MACRO(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & O_APPEND);
    int fd;
    int *retval;
    errno = 0;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~O_APPEND;

    fd = open(filename, mode, S_IRUSR | S_IWUSR);
    BAIL_IF_MACRO(fd < 0, errcodeFromErrno(), NULL);

    if (appending)
    {
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            const int err = errno;
            close(fd);
            BAIL_MACRO(errcodeFromErrnoError(err), NULL);
        } /* if */
    } /* if */

    retval = (int *) physfs_alloc.Malloc(sizeof (int));
    if (!retval)
    {
        close(fd);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return ((void *) retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, O_RDONLY);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_TRUNC);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_APPEND);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    rc = read(fd, buffer, (size_t) len);
    BAIL_IF_MACRO(rc == -1, errcodeFromErrno(), -1);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    rc = write(fd, (void *) buffer, (size_t) len);
    BAIL_IF_MACRO(rc == -1, errcodeFromErrno(), rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    const int fd = *((int *) opaque);
    const int rc = lseek(fd, (off_t) pos, SEEK_SET);
    BAIL_IF_MACRO(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    const int fd = *((int *) opaque);
    PHYSFS_sint64 retval;
    retval = (PHYSFS_sint64) lseek(fd, 0, SEEK_CUR);
    BAIL_IF_MACRO(retval == -1, errcodeFromErrno(), -1);
    return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    const int fd = *((int *) opaque);
    struct stat statbuf;
    BAIL_IF_MACRO(fstat(fd, &statbuf) == -1, errcodeFromErrno(), -1);
    return ((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    const int fd = *((int *) opaque);
    if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY)
        BAIL_IF_MACRO(fsync(fd) == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    const int fd = *((int *) opaque);
    (void) close(fd);  /* we don't check this. You should have used flush! */
    physfs_alloc.Free(opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF_MACRO(remove(path) == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformDelete */


int __PHYSFS_platformStat(const char *filename, PHYSFS_Stat *st)
{
    struct stat statbuf;

    BAIL_IF_MACRO(lstat(filename, &statbuf) == -1, errcodeFromErrno(), 0);

    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if(S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    else if(S_ISLNK(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_SYMLINK;
        st->filesize = 0;
    } /* else if */

    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } /* else */

    st->modtime = statbuf.st_mtime;
    st->createtime = statbuf.st_ctime;
    st->accesstime = statbuf.st_atime;

    /* !!! FIXME: maybe we should just report full permissions? */
    st->readonly = access(filename, W_OK);
    return 1;
} /* __PHYSFS_platformStat */


#ifndef PHYSFS_PLATFORM_BEOS  /* BeOS has its own code in platform_beos.cpp */
#if (defined PHYSFS_NO_THREAD_SUPPORT)

void *__PHYSFS_platformGetThreadID(void) { return ((void *) 0x0001); }
void *__PHYSFS_platformCreateMutex(void) { return ((void *) 0x0001); }
void __PHYSFS_platformDestroyMutex(void *mutex) {}
int __PHYSFS_platformGrabMutex(void *mutex) { return 1; }
void __PHYSFS_platformReleaseMutex(void *mutex) {}

#else

typedef struct
{
    pthread_mutex_t mutex;
    pthread_t owner;
    PHYSFS_uint32 count;
} PthreadMutex;


void *__PHYSFS_platformGetThreadID(void)
{
    return ( (void *) ((size_t) pthread_self()) );
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    int rc;
    PthreadMutex *m = (PthreadMutex *) physfs_alloc.Malloc(sizeof (PthreadMutex));
    BAIL_IF_MACRO(!m, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    rc = pthread_mutex_init(&m->mutex, NULL);
    if (rc != 0)
    {
        physfs_alloc.Free(m);
        BAIL_MACRO(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    m->count = 0;
    m->owner = (pthread_t) 0xDEADBEEF;
    return ((void *) m);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;

    /* Destroying a locked mutex is a bug, but we'll try to be helpful. */
    if ((m->owner == pthread_self()) && (m->count > 0))
        pthread_mutex_unlock(&m->mutex);

    pthread_mutex_destroy(&m->mutex);
    physfs_alloc.Free(m);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;
    pthread_t tid = pthread_self();
    if (m->owner != tid)
    {
        if (pthread_mutex_lock(&m->mutex) != 0)
            return 0;
        m->owner = tid;
    } /* if */

    m->count++;
    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;
    assert(m->owner == pthread_self());  /* catch programming errors. */
    assert(m->count > 0);  /* catch programming errors. */
    if (m->owner == pthread_self())
    {
        if (--m->count == 0)
        {
            m->owner = (pthread_t) 0xDEADBEEF;
            pthread_mutex_unlock(&m->mutex);
        } /* if */
    } /* if */
} /* __PHYSFS_platformReleaseMutex */

#endif /* !PHYSFS_NO_THREAD_SUPPORT */
#endif /* !PHYSFS_PLATFORM_BEOS */

#endif  /* PHYSFS_PLATFORM_POSIX */

/* end of posix.c ... */

