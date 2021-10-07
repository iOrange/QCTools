#include "TPLResource.h"

#include <fstream>
#include <string>

namespace qc {

#pragma pack (push, 1)
struct TPLHeader {          // sizeof == 64
    u32  magic;             // 'RES1'
    u32  resType;           // tpl'\0'
    u8   nulls[28];         // nulls
    char s3dresource[28];   // s3d resource signature
};
#pragma pack (pop)

TPLResource::TPLResource() {
}
TPLResource::~TPLResource() {
}

bool TPLResource::LoadFromData(const u8* data, u64 length) {
    bool result = false;

    const u8* ptr = data;

    const TPLHeader* hdr = reinterpret_cast<const TPLHeader*>(ptr);
    ptr += sizeof(TPLHeader);

    if (hdr->magic == qc::fourcc<'1', 'S', 'E', 'R'>::value &&
        hdr->resType == qc::fourcc<'t', 'p', 'l', 0>::value) {

        const u32 tplContainerMagic = *reinterpret_cast<const u32*>(ptr);
        ptr += sizeof(u32);

        if (tplContainerMagic == qc::fourcc<'T', 'P', 'L', '1'>::value) {
            ptr += 6; // !!SKIP 6 unkown bytes

            const u32 tplResNameLen = *reinterpret_cast<const u32*>(ptr);
            ptr += sizeof(u32);

            mTPLResName.assign(reinterpret_cast<const char*>(ptr), tplResNameLen);
            ptr += tplResNameLen;

            ptr += 6; // !!SKIP 6 unkown bytes

            const u32 tplResDescLen = *reinterpret_cast<const u32*>(ptr);
            ptr += sizeof(u32);

            mTPLResDesc.assign(reinterpret_cast<const char*>(ptr), tplResDescLen);
            ptr += tplResDescLen;

            result = true;
        }
    }

    return result;
}

bool TPLResource::LoadFromFile(const String& fileName) {
    bool result = false;

    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.good()) {
        file.seekg(0, std::ios_base::end);
        const u64 fileSize = file.tellg();
        file.seekg(0, std::ios_base::beg);
        Array<u8> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        result = this->LoadFromData(buffer.data(), fileSize);
    }

    return result;
}

} // namespace qc
