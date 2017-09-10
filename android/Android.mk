LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := physfs
LOCAL_CFLAGS := -O3 -D_REENTRANT -D_THREAD_SAFE \
	-DPHYSFS_NO_CDROM_SUPPORT -DPHYSFS_SUPPORTS_7Z \
	-DPHYSFS_SUPPORTS_MPQ -DPHYSFS_SUPPORTS_ZIP

LOCAL_CPPFLAGS := ${LOCAL_CFLAGS}

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

LOCAL_SRC_FILES := \
	$(LOCAL_PATH)/src/physfs.c \
	$(LOCAL_PATH)/src/physfs_archiver_7z.c \
	$(LOCAL_PATH)/src/physfs_archiver_dir.c \
	$(LOCAL_PATH)/src/physfs_archiver_grp.c \
	$(LOCAL_PATH)/src/physfs_archiver_hog.c \
	$(LOCAL_PATH)/src/physfs_archiver_iso9660.c \
	$(LOCAL_PATH)/src/physfs_archiver_mpq.c \
	$(LOCAL_PATH)/src/physfs_archiver_mvl.c \
	$(LOCAL_PATH)/src/physfs_archiver_qpak.c \
	$(LOCAL_PATH)/src/physfs_archiver_slb.c \
	$(LOCAL_PATH)/src/physfs_archiver_unpacked.c \
	$(LOCAL_PATH)/src/physfs_archiver_vdf.c \
	$(LOCAL_PATH)/src/physfs_archiver_wad.c \
	$(LOCAL_PATH)/src/physfs_archiver_zip.c \
	$(LOCAL_PATH)/src/physfs_byteorder.c \
	$(LOCAL_PATH)/src/physfs_platform_posix.c \
	$(LOCAL_PATH)/src/physfs_platform_unix.c \
	$(LOCAL_PATH)/src/physfs_unicode.c \
	$(LOCAL_PATH)/src/StormLib/FileStream.cpp \
	$(LOCAL_PATH)/src/StormLib/SBaseCommon.cpp \
	$(LOCAL_PATH)/src/StormLib/SBaseFileTable.cpp \
	$(LOCAL_PATH)/src/StormLib/SCompression.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileGetFileInfo.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileOpenArchive.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileOpenFileEx.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileReadFile.cpp \
	$(LOCAL_PATH)/src/StormLib/pklib/explode.c \

include $(BUILD_STATIC_LIBRARY)
