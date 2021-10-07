#pragma once

#include "qccommon.h"

namespace qc {

#pragma pack(push, 1)

struct ZipCentraDirectoryHeader {
    enum : u32 {
        MAGIC = fourcc<'P','K',1,2>::value
    };

    u32 magic;
    u16 versionMade;
    u16 versionNeeded;
    u16 flags;
    u16 compression;
    u16 time;
    u16 date;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 nameLength;
    u16 extraLength;
    u16 commentLength;
    u16 diskNumber;
    u16 internalAttribs;
    u32 externalAttribs;
    u32 firstFileOffset;
};

struct ZipLocalFileHeader {
    enum : u32 {
        MAGIC = fourcc<'P','K',3,4>::value
    };

    u32 magic;
    u16 version;
    u16 flags;
    u16 compression;
    u16 time;
    u16 date;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 nameLength;
    u16 extraLength;
};

struct ZipEndOfCentralDirectory {
    enum : u32 {
        MAGIC = fourcc<'P','K',5,6>::value
    };

    u32 magic;
    u16 diskNumber;
    u16 startDisk;
    u16 filesNum;
    u16 totalFiles;
    u32 size;
    u32 offset;
    u16 commentLength;
};

struct ZipDataDescriptorHeader {
    enum : u32 {
        MAGIC = fourcc<'P','K',7,8>::value
    };

    u32 magic;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
};

struct Zip64EOCDLocator {
    enum : u32 {
        MAGIC = fourcc<'P','K',6,7>::value
    };

    u32 magic;
    u32 disk;
    u64 eocdRecordOffset;
    u32 totalDisks;
};

struct Zip64EOCDRecord {
    enum : u32 {
        MAGIC = fourcc<'P','K',6,6>::value
    };

    u32 magic;
    u64 eocdSize;
    u16 versionMade;
    u16 versionNeeded;
    u32 diskNumber;
    u32 startDisk;
    u64 filesOnDisk;
    u64 filesInCD;
    u64 cdSize;
    u64 cdOffset;
};


#pragma pack(pop)

} // namespace qc
