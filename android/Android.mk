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
	$(LOCAL_PATH)/src/archiver_dir.c \
	$(LOCAL_PATH)/src/archiver_grp.c \
	$(LOCAL_PATH)/src/archiver_hog.c \
	$(LOCAL_PATH)/src/archiver_iso9660.c \
	$(LOCAL_PATH)/src/archiver_lzma.c \
	$(LOCAL_PATH)/src/archiver_mpq.c \
	$(LOCAL_PATH)/src/archiver_mvl.c \
	$(LOCAL_PATH)/src/archiver_qpak.c \
	$(LOCAL_PATH)/src/archiver_slb.c \
	$(LOCAL_PATH)/src/archiver_unpacked.c \
	$(LOCAL_PATH)/src/archiver_wad.c \
	$(LOCAL_PATH)/src/archiver_zip.c \
	$(LOCAL_PATH)/src/physfs.c \
	$(LOCAL_PATH)/src/physfs_byteorder.c \
	$(LOCAL_PATH)/src/physfs_unicode.c \
	$(LOCAL_PATH)/src/platform_beos.cpp \
	$(LOCAL_PATH)/src/platform_macosx.c \
	$(LOCAL_PATH)/src/platform_posix.c \
	$(LOCAL_PATH)/src/platform_unix.c \
	$(LOCAL_PATH)/src/platform_windows.c \
	$(LOCAL_PATH)/src/platform_winrt.cpp \
	$(LOCAL_PATH)/src/lzma/C/7zCrc.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zBuffer.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zDecode.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zExtract.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zHeader.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zIn.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zItem.c \
	$(LOCAL_PATH)/src/lzma/C/Archive/7z/7zMethodID.c \
	$(LOCAL_PATH)/src/lzma/C/Compress/Branch/BranchX86.c \
	$(LOCAL_PATH)/src/lzma/C/Compress/Branch/BranchX86_2.c \
	$(LOCAL_PATH)/src/lzma/C/Compress/Lzma/LzmaDecode.c \
	$(LOCAL_PATH)/src/StormLib/FileStream.cpp \
	$(LOCAL_PATH)/src/StormLib/SBaseCommon.cpp \
	$(LOCAL_PATH)/src/StormLib/SBaseFileTable.cpp \
	$(LOCAL_PATH)/src/StormLib/SCompression.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileFindFile.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileGetFileInfo.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileListFile.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileOpenArchive.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileOpenFileEx.cpp \
	$(LOCAL_PATH)/src/StormLib/SFileReadFile.cpp \
	$(LOCAL_PATH)/src/StormLib/pklib/explode.c \

include $(BUILD_STATIC_LIBRARY)
