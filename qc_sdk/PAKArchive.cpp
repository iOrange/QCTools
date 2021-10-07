#include "PAKArchive.h"

#include <cstring>
#include <assert.h>

#include <filesystem>
namespace fs = std::experimental::filesystem::v1;

#include "zipcommon.h"

#include <zlib/zlib.h>

namespace qc {


static int DecompressMemoryRAW(u8* data_in, const u64 size_in, u8* data_out, const u64 size_out) {
    z_stream zs;

    zs.zalloc = (alloc_func)0;
    zs.zfree = (free_func)0;
    zs.opaque = (voidpf)0;

    zs.next_in = data_in;
    zs.avail_in = static_cast<uInt>(size_in);

    zs.next_out = data_out;
    zs.avail_out = static_cast<uInt>(size_out);

    int err = inflateInit2(&zs, -15);
    if (err != Z_OK) {
        return 0;
    }

    err = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);

    if (zs.total_out != size_out) {
        return 0;
    } else {
        return static_cast<int>(size_out);
    }
}


PAKArchive::PAKArchive()
    : mFileSize(0)
{
}
PAKArchive::~PAKArchive() {
}

bool PAKArchive::LoadFromFile(const String& fileName) {
    bool result = false;

    mFile.open(fileName, std::ios_base::in | std::ios_base::binary);
    if (mFile.good()) {
        fs::path p(fileName);
        mName = p.filename().replace_extension().string();

        mFile.seekg(0, std::ios_base::end);
        mFileSize = static_cast<u64>(mFile.tellg());
        mFile.seekg(0);

        // read archive header
        mFile.seekg(mFileSize - sizeof(PAKHeader));
        PAKHeader pakHdr;
        mFile.read(reinterpret_cast<char*>(&pakHdr), sizeof(pakHdr));

        this->InitDecipher(reinterpret_cast<const u8*>(&pakHdr));

        // find EOCD record
        const u64 searchDistance = 0x1002A;
        mFile.seekg(mFileSize - searchDistance);
        Array<u8> buffer(searchDistance);
        mFile.read(reinterpret_cast<char*>(buffer.data()), searchDistance);
        const u8* ptrOECD = this->FindEOCD(buffer.data(), searchDistance);
        if (ptrOECD) {
            u64 eocdOffset = searchDistance - static_cast<u64>(ptrOECD - buffer.data());
            eocdOffset = mFileSize - eocdOffset;

            ZipEndOfCentralDirectory eocd;
            std::memcpy(&eocd, ptrOECD, sizeof(eocd));

            mCDStartOffset = eocd.offset;
            mCDEndOffset = eocdOffset;
            mFilesNumber = eocd.totalFiles;

            if (mCDStartOffset == 0xffffffff) { // Zip64
                const u8* zip64EOCDPtr = ptrOECD - sizeof(Zip64EOCDLocator);
                if (*reinterpret_cast<const u32*>(zip64EOCDPtr) == Zip64EOCDLocator::MAGIC) {
                    Zip64EOCDLocator eocdLocator;
                    std::memcpy(&eocdLocator, zip64EOCDPtr, sizeof(Zip64EOCDLocator));
                    const u64 zip64EOCDOffset = eocdLocator.eocdRecordOffset;

                    mFile.seekg(zip64EOCDOffset);
                    Zip64EOCDRecord eocdRecord64;
                    mFile.read(reinterpret_cast<char*>(&eocdRecord64), sizeof(eocdRecord64));
                    mCDStartOffset = eocdRecord64.cdOffset;
                    mFilesNumber = eocdRecord64.filesInCD;
                }
            }


            // goto CD start
            mFile.seekg(mCDStartOffset);

            mTOC.resize(mFilesNumber);
            for (auto& entry : mTOC) {
                ZipCentraDirectoryHeader cdHdr;
                mFile.read(reinterpret_cast<char*>(&cdHdr), sizeof(cdHdr));
                this->DecipherBytes(reinterpret_cast<u8*>(&cdHdr), sizeof(cdHdr));

                Array<char> name(cdHdr.nameLength);
                mFile.read(name.data(), name.size());
                this->DecipherBytes(reinterpret_cast<u8*>(name.data()), name.size());
                entry.fileName.assign(name.data(), name.size());

                Array<u8> extra(cdHdr.extraLength);
                mFile.read(reinterpret_cast<char*>(extra.data()), extra.size());

                entry.crc32 = cdHdr.crc32;
                entry.compressedSize = cdHdr.compressedSize;
                entry.uncompressedSize = cdHdr.uncompressedSize;
                entry.offset = cdHdr.firstFileOffset;

                if (entry.offset == 0xffffffff && extra.size() >= 12) { // Zip64
                    u16 extraId, extraLen;
                    std::memcpy(&extraId, extra.data(), 2);
                    std::memcpy(&extraLen, extra.data() + 2, 2);
                    if (extraId == 1 && extraLen == 8) {
                        memcpy(&entry.offset, extra.data() + 4, sizeof(u64));
                    }
                }
            }

            result = true;
        }
    }

    return result;
}


const String& PAKArchive::GetPAKName() const {
    return mName;
}

u64 PAKArchive::GetNumFiles() const {
    return mTOC.size();
}

const String& PAKArchive::GetFileName(const u64 i) const {
    assert(i >= 0 && i < mTOC.size());
    return mTOC[i].fileName;
}

u64 PAKArchive::GetFileOffset(const u64 i) const {
    assert(i >= 0 && i < mTOC.size());
    return mTOC[i].offset;
}

u64 PAKArchive::GetFileCompressedSize(const u64 i) const {
    assert(i >= 0 && i < mTOC.size());
    return mTOC[i].compressedSize;
}

u64 PAKArchive::GetFileUncompressedSize(const u64 i) const {
    assert(i >= 0 && i < mTOC.size());
    return mTOC[i].uncompressedSize;
}

Array<u8> PAKArchive::GetFileCompressedData(const u64 i) {
    Array<u8> result;

    const u64 compressedSize = this->GetFileCompressedSize(i);
    if (compressedSize && mFile.good()) {
        const u64 offset = this->GetFileOffset(i);
        result.resize(compressedSize);
        mFile.seekg(offset + 30);
        mFile.read(reinterpret_cast<char*>(result.data()), compressedSize);
    }

    return std::move(result);
}

Array<u8> PAKArchive::GetFileUncompressedData(const u64 i) {
    Array<u8> result;

    const u64 uncompressedSize = this->GetFileUncompressedSize(i);
    if (uncompressedSize && mFile.good()) {
        const u64 compressedSize = this->GetFileCompressedSize(i);
        Array<u8> compressed = this->GetFileCompressedData(i);
        if (compressedSize != uncompressedSize) {
            if (!compressed.empty()) {
                result.resize(uncompressedSize);
                DecompressMemoryRAW(compressed.data(), compressed.size(), result.data(), uncompressedSize);
            }
        } else {
            result.swap(compressed);
        }
    }

    return std::move(result);
}



void PAKArchive::InitDecipher(const u8* key) {
    memset(&mDecipher, 0, sizeof(mDecipher));
    memcpy(mDecipher.key2, key, 32);
    memcpy(&mDecipher.key2[32], key, 32);
    memcpy(&mDecipher.key_part, key, 8);

    mDecipher.rnd.Init(*reinterpret_cast<const u64*>(key));
}

const u8* PAKArchive::FindEOCD(const u8* data, const u64 dataLength) const {
    const u8* ptr = reinterpret_cast<const u8*>(std::memchr(data, 'P', dataLength));
    while (ptr && (ZipEndOfCentralDirectory::MAGIC != *reinterpret_cast<const u32*>(ptr))) {
        const u8* nextPtr = ptr + 1;
        const u64 pos = nextPtr - data;
        ptr = reinterpret_cast<const u8*>(std::memchr(nextPtr, 'P', dataLength - pos));
    }
    return ptr;
}

void PAKArchive::DecipherBytes(u8* data, const u64 dataLength) {
    u8* ptr = data;

    for (u64 i = 0; i < dataLength; ++i, ++ptr) {
        const u8 k = mDecipher.key2[32 + mDecipher.counter32];
        u8 p = *ptr;
        mDecipher.key2[32 + mDecipher.counter32] = p;

        const u64 mask = 0xff;
        p ^= (k ^ (mDecipher.key_part[0] & (mask << (8 * mDecipher.counter8))));
        *ptr = p;

        mDecipher.counter32 = (mDecipher.counter32 + 1) % 32;
        mDecipher.counter8++;

        if (mDecipher.counter8 == 8) {
            const u64 next = mDecipher.rnd.Get64();
            memcpy(mDecipher.key_part, &next, sizeof(next));
            mDecipher.counter8 = 0;
        }
    }
}

} // namespace qc
