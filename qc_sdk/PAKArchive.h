#pragma once

#include "qccommon.h"
#include <fstream>

namespace qc {

struct PAKHeader {
    enum : u64 {
        MAGIC = 0x31534B0644065031
    };

    u64 rnd0;
    u64 rnd1;
    u64 rnd2;
    u64 rnd3;
    u64 magic;
};


struct TOCEntry {
    String fileName;
    u32 crc32;
    u64 compressedSize;
    u64 uncompressedSize;
    u64 offset;
};

class PAKArchive {
    struct DecipherStruct {
        u8 key2[64];
        u8 key_part[8];
        u16 counter8;
        u16 counter32;

        Random64 rnd;
    };

public:
    PAKArchive();
    ~PAKArchive();

    bool            LoadFromFile(const String& fileName);

    const String&   GetPAKName() const;
    u64             GetNumFiles() const;
    const String&   GetFileName(const u64 i) const;
    u64             GetFileOffset(const u64 i) const;
    u64             GetFileCompressedSize(const u64 i) const;
    u64             GetFileUncompressedSize(const u64 i) const;
    Array<u8>       GetFileCompressedData(const u64 i);
    Array<u8>       GetFileUncompressedData(const u64 i);

private:
    void            InitDecipher(const u8* key);
    const u8*       FindEOCD(const u8* data, const u64 dataLength) const;
    void            DecipherBytes(u8* data, const u64 dataLength);

private:
    String          mName;
    std::ifstream   mFile;
    u64             mFileSize;
    u64             mCDStartOffset;
    u64             mCDEndOffset;
    u64             mFilesNumber;

    Array<TOCEntry> mTOC;

    DecipherStruct  mDecipher;
};

} // namespace qc
