#pragma once

#include "qccommon.h"

namespace qc {

#pragma pack (push, 1)
struct PCTHeader {          // sizeof == 58
    char unk_01[16];
    int  width;
    int  height;
    int  depth;
    int  numImages;         // cubemaps ???
    char unk_02[6];
    int  texFormat;         // ETexFormat
    char unk_03[6];
    int  numMips;
    char unk_04[2];
    int  fileSizeNoFooter;  // file size - 6
};
#pragma pack (pop)

enum ETexFormat : int {
    TF_RGBA = 0x00,     // uncompressed RGBA
    TF_BC1  = 0x0C,
    TF_BC3  = 0x11,
    TF_RGBX = 0x16,     // uncompressed RGBx (alpha channel should be ignored)
    TF_BC5  = 0x21,
    TF_BC5X = 0x24,     // seems to be the exact same BC5 (ATI2) [wtf ???]
    TF_BC4  = 0x25,
    TF_BC6H = 0x31,
    TF_BC7  = 0x33,
    TF_BC7X = 0x34      // seems to be the exact same BC7 [wtf ???]
};

struct SubImageData {
    int         width;
    int         height;
    ETexFormat  format;
    Array<u8>   data;
};

class PCTTexture {
public:
    PCTTexture();
    ~PCTTexture();

    bool            LoadFromData(const u8* data, const u64 length);
    bool            LoadFromFile(const String& fileName);
    bool            SaveToFile(const String& fileName);

    const String&   GetFileName() const;

    int             GetWidth() const;
    int             GetHeight() const;
    ETexFormat      GetFormat() const;
    int             GetMipsCount() const;
    bool            IsCubemap() const;
    bool            IsHDR() const;

    void            ReplaceUsingRGBA(const u8* data);
    bool            ReplaceUsingDDS(const String& fileName);

    Array<u8>       UnpackToRGBA(int imgIdx, bool bgra = false) const;
    Array<u8>       UnpackToRGBE(int imgIdx) const;

    bool            ExportToTGA(const String& fileName);
    bool            ExportToDDS(const String& fileName);
    bool            ExportToHDR(const String& fileName);

private:
    String              mFileName;
    PCTHeader           mHeader;
    Array<SubImageData> mImages;
    bool                mIsCubemap;
    int                 mMipsCount;
};

} // namespace qc
