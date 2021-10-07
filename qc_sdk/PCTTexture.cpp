#include "PCTTexture.h"

#include <DirectXTex/DirectXTex.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"

#include <fstream>
#include <string>


namespace qc {

enum : int {
    kFooterSize = 6
};

inline bool IsCompressed(const int fmt) {
    return fmt != TF_RGBA && fmt != TF_RGBX;
}

inline int GetCompressedSize(const int width, const int height, const ETexFormat fmt) {
    int result = 0;

    if (!IsCompressed(fmt)) {
        result = width * height * 4;
    } else {
        // compute the storage requirements
        const int blockcount = ((width + 3) / 4) * ((height + 3) / 4);
        const int blocksize = (fmt == TF_BC4 || fmt == TF_BC1) ? 8 : 16;
        result = blockcount * blocksize;
    }

    return result;
}

inline DXGI_FORMAT TexFormat2DxgiFormat(const int fmt) {
    DXGI_FORMAT dxFormat = DXGI_FORMAT_BC3_UNORM;

    if (fmt == TF_RGBA || fmt == TF_RGBX) {
        dxFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    } else if (fmt == TF_BC1) {
        dxFormat = DXGI_FORMAT_BC1_UNORM;
    } else if (fmt == TF_BC4) {
        dxFormat = DXGI_FORMAT_BC4_UNORM;
    } else if (fmt == TF_BC5 || fmt == TF_BC5X) {
        dxFormat = DXGI_FORMAT_BC5_UNORM;
    } else if (fmt == TF_BC6H) {
        dxFormat = DXGI_FORMAT_BC6H_UF16;
    } else if (fmt == TF_BC7 || fmt == TF_BC7X) {
        dxFormat = DXGI_FORMAT_BC7_UNORM;
    }

    return dxFormat;
}




PCTTexture::PCTTexture()
    : mIsCubemap(false)
    , mMipsCount(1)
{
}
PCTTexture::~PCTTexture() {
}


bool PCTTexture::LoadFromData(const u8* data, const u64 length) {
    const u8* ptr = data;
    mHeader = *reinterpret_cast<const PCTHeader*>(ptr);
    ptr += sizeof(PCTHeader);

    u64 hdrSize = sizeof(PCTHeader);

    if (mHeader.unk_04[0] != 0xFF) {
        hdrSize = mHeader.fileSizeNoFooter;
        memcpy(&mHeader.unk_04[0], data + hdrSize, 6);
        hdrSize += 6;
        ptr = data + hdrSize;
    }

    ETexFormat fmt = static_cast<ETexFormat>(mHeader.texFormat);
    mIsCubemap = mHeader.numImages == 6;

    int w = mHeader.width;
    int h = mHeader.height;
    int numMips = mHeader.numMips;
    if (numMips > 1) {
        // let's verify that
        u64 totalMipsSizeCompressed = 0;

        for (int i = 0; i < numMips; ++i) {
            // mip size can't be less than the block size
            const int actualMipW = !IsCompressed(fmt) ? w : ((w < 4) ? 4 : w);
            const int actualMipH = !IsCompressed(fmt) ? h : ((h < 4) ? 4 : h);

            const int mipSize = GetCompressedSize(actualMipW, actualMipH, fmt);

            totalMipsSizeCompressed += (mipSize * mHeader.numImages);

            w >>= 1;
            h >>= 1;
        }

        if ((totalMipsSizeCompressed + hdrSize + kFooterSize) != length) {
            // something is wrong here, let's just pick the first mip
            numMips = 1;
        }
    }

    mMipsCount = numMips;

    w = mHeader.width;
    h = mHeader.height;
    for (int i = 0; i < numMips; ++i) {
        // mip size can't be less than the block size
        const int actualMipW = !IsCompressed(fmt) ? w : ((w < 4) ? 4 : w);
        const int actualMipH = !IsCompressed(fmt) ? h : ((h < 4) ? 4 : h);

        const int mipSize = GetCompressedSize(actualMipW, actualMipH, fmt);

        for (int j = 0; j < mHeader.numImages; ++j) {
            SubImageData img;
            img.width = w;
            img.height = h;
            img.format = fmt;
            img.data.reserve(mipSize);
            img.data.assign(ptr, ptr + mipSize);
            mImages.push_back(img);
        }

        w >>= 1;
        h >>= 1;

        ptr += mipSize;
    }

    return true;
}

bool PCTTexture::LoadFromFile(const String& fileName) {
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

        mFileName = fileName;
    }

    return result;
}

bool PCTTexture::SaveToFile(const String& fileName) {
    bool result = false;

    std::ofstream file(fileName, std::ios_base::out | std::ios_base::binary);
    if (file.good()) {
        file.write(reinterpret_cast<char*>(&mHeader), sizeof(PCTHeader));

        int w = mHeader.width;
        int h = mHeader.height;
        int imageIdx = 0;
        for (int i = 0; i < mHeader.numMips; ++i) {
            // mip size can't be less than the block size
            const int actualMipW = !IsCompressed(mHeader.texFormat) ? w : ((w < 4) ? 4 : w);
            const int actualMipH = !IsCompressed(mHeader.texFormat) ? h : ((h < 4) ? 4 : h);

            const int mipSize = GetCompressedSize(actualMipW, actualMipH, static_cast<ETexFormat>(mHeader.texFormat));

            for (int j = 0; j < mHeader.numImages; ++j, ++imageIdx) {
                file.write(reinterpret_cast<char*>(mImages[imageIdx].data.data()), mipSize);
            }

            w >>= 1;
            h >>= 1;
        }

        // footer 0x01 0x00 4_bytes_file_length
        file.put(1);
        file.put(0);
        i32 fullFileSize = mHeader.fileSizeNoFooter + kFooterSize;
        file.write(reinterpret_cast<char*>(&fullFileSize), sizeof(fullFileSize));

        file.close();

        result = true;
    }

    return result;
}

const String& PCTTexture::GetFileName() const {
    return mFileName;
}

int PCTTexture::GetWidth() const {
    return mImages[0].width;
}

int PCTTexture::GetHeight() const {
    return mImages[0].height;
}

ETexFormat PCTTexture::GetFormat() const {
    return mImages[0].format;
}

int PCTTexture::GetMipsCount() const {
    return mMipsCount;
}

bool PCTTexture::IsCubemap() const {
    return mIsCubemap;
}

bool PCTTexture::IsHDR() const {
    return this->GetFormat() == TF_BC6H;
}

void PCTTexture::ReplaceUsingRGBA(const u8* data) {
    int w = mHeader.width;
    int h = mHeader.height;
    int imageIdx = 0;

    Array<u8> resized((w / 2) * (h / 2) * 4);
    u8* resizedPtr = resized.data();

    for (int i = 0; i < mHeader.numMips; ++i, ++imageIdx) {
        // mip size can't be less than the block size
        const int actualMipW = !IsCompressed(mHeader.texFormat) ? w : ((w < 4) ? 4 : w);
        const int actualMipH = !IsCompressed(mHeader.texFormat) ? h : ((h < 4) ? 4 : h);
        const int mipSize = GetCompressedSize(actualMipW, actualMipH, static_cast<ETexFormat>(mHeader.texFormat));

        const u8* srcImage = data;
        if (i) {
            stbir_resize_uint8(data, mHeader.width, mHeader.height, 0, resizedPtr, actualMipW, actualMipH, 0, 4);
            srcImage = resizedPtr;
        }

        if (!IsCompressed(mHeader.texFormat)) {
            memcpy(mImages[imageIdx].data.data(), srcImage, mImages[imageIdx].data.size());
        } else {
            const DXGI_FORMAT dxFormat = TexFormat2DxgiFormat(mHeader.texFormat);

            DirectX::Image srcImg = { static_cast<size_t>(w), static_cast<size_t>(h), DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, (uint8_t*)srcImage };
            DirectX::ComputePitch(srcImg.format, srcImg.width, srcImg.height, srcImg.rowPitch, srcImg.slicePitch);
            DirectX::ScratchImage dstImg;
            DirectX::Compress(srcImg, dxFormat, DirectX::TEX_COMPRESS_DEFAULT | DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, dstImg);

            assert((size_t)mipSize == mImages[imageIdx].data.size());
            memcpy(mImages[imageIdx].data.data(), dstImg.GetPixels(), mImages[imageIdx].data.size());
        }

        w >>= 1;
        h >>= 1;
    }
}

bool PCTTexture::ReplaceUsingDDS(const String& fileName) {
    bool result = false;

    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.good()) {
        file.seekg(0, std::ios_base::end);
        const u64 fileSize = file.tellg();
        file.seekg(0, std::ios_base::beg);
        Array<u8> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        DirectX::TexMetadata meta;
        DirectX::ScratchImage ddsImage;
        HRESULT hr = DirectX::LoadFromDDSMemory(buffer.data(), buffer.size(), DirectX::DDS_FLAGS_NONE, &meta, ddsImage);
        if (SUCCEEDED(hr)) {
            if (meta.width >= mHeader.width && meta.height >= mHeader.height) {
                const DXGI_FORMAT dxFormat = TexFormat2DxgiFormat(mHeader.texFormat);

                Array<DirectX::Image> mips; mips.resize(meta.mipLevels);
                if (dxFormat != meta.format) {
                    for (size_t i = 0; i < meta.mipLevels; ++i) {
                        DirectX::ScratchImage tmpImg;
                        const DirectX::Image& mipImg = *ddsImage.GetImage(i, 0, 0);
                        hr = DirectX::Convert(mipImg, dxFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, tmpImg);
                        if (FAILED(hr)){
                            return false;
                        }

                        DirectX::Image& img = mips[i];
                        img.format = dxFormat;
                        img.width = mipImg.width;
                        img.height = mipImg.height;
                        img.rowPitch = mipImg.rowPitch;
                        img.slicePitch = mipImg.slicePitch;
                        memcpy(img.pixels, mipImg.pixels, mipImg.slicePitch);
                    }
                } else {
                    for (size_t i = 0; i < meta.mipLevels; ++i) {
                        DirectX::Image& img = mips[i];
                        img = *ddsImage.GetImage(i, 0, 0);
                    }
                }

                const size_t startMip = mips.size() - mHeader.numMips;
                for (size_t i = startMip, j = 0; i < mips.size(); ++i, ++j) {
                    const DirectX::Image& img = mips[i];
                    assert(img.slicePitch == mImages[j].data.size());
                    memcpy(mImages[j].data.data(), img.pixels, img.slicePitch);
                }

                result = true;
            }
        }
    }

    return result;
}

Array<u8> PCTTexture::UnpackToRGBA(int imgIdx, bool bgra) const {
    const SubImageData& img = mImages[imgIdx];

    const int w = img.width;
    const int h = img.height;
    const ETexFormat fmt = img.format;
    const u8* data = img.data.data();

    Array<u8> result(w * h * 4);

    if (fmt == TF_RGBA) {
        std::memmove(result.data(), data, result.size());
    } else if (fmt == TF_RGBX) {
        const u32* src = reinterpret_cast<const u32*>(data);
        u32* dst = reinterpret_cast<u32*>(result.data());
        for (int i = 0; i < w * h; ++i, ++dst, ++src) {
            *dst = *src | 0xff000000;
        }
    } else {
        const DXGI_FORMAT dxFormat = TexFormat2DxgiFormat(fmt);

        DirectX::Image srcImg = { static_cast<size_t>(w), static_cast<size_t>(h), dxFormat, 0, 0, (uint8_t*)data };
        DirectX::ComputePitch(srcImg.format, srcImg.width, srcImg.height, srcImg.rowPitch, srcImg.slicePitch);
        DirectX::ScratchImage dstImg;
        DirectX::Decompress(srcImg, bgra ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM, dstImg);

        memcpy(result.data(), dstImg.GetPixels(), result.size());
    }

    return std::move(result);
}

Array<u8> PCTTexture::UnpackToRGBE(int imgIdx) const {
    const SubImageData& img = mImages[imgIdx];

    const int w = img.width;
    const int h = img.height;
    const ETexFormat fmt = img.format;
    const u8* data = img.data.data();

    Array<u8> result(w * h * 4);

    const DXGI_FORMAT dxFormat = TexFormat2DxgiFormat(fmt);

    DirectX::Image srcImg = { static_cast<size_t>(w), static_cast<size_t>(h), dxFormat, 0, 0, (uint8_t*)data };
    DirectX::ComputePitch(srcImg.format, srcImg.width, srcImg.height, srcImg.rowPitch, srcImg.slicePitch);
    DirectX::ScratchImage dstImg;
    DirectX::Decompress(srcImg, DXGI_FORMAT_R32G32B32_FLOAT, dstImg);

    // Unfortunately, DirectXTex can only unpack to either r9g9b9e5 or rgb_float
    // and radiance hdr format requires r8g8b8e8
    // so let's convert it manually

    const float* ptrFloat = reinterpret_cast<const float*>(dstImg.GetPixels());
    u32* ptrRGBE = reinterpret_cast<u32*>(result.data());
    for (int i = 0; i < w * h; ++i) {
        const float x = ptrFloat[0];
        const float y = ptrFloat[1];
        const float z = ptrFloat[2];

        float v = x > y ? x : y;
        v = v > z ? v : z;

        if (v < 1e-32f) {
            *ptrRGBE = 0;
        } else {
            int ex;
            const float m = std::frexpf(v, &ex) * 256.0f / v;

            const u32 r = static_cast<u32>(m * x);
            const u32 g = static_cast<u32>(m * y);
            const u32 b = static_cast<u32>(m * z);
            const u32 e = static_cast<u32>(ex + 128);

            *ptrRGBE = r | (g << 8) | (b << 16) | (e << 24);
        }

        ptrFloat += 3;
        ptrRGBE++;
    }

    return std::move(result);
}

bool PCTTexture::ExportToTGA(const String& fileName) {
    bool result = false;

    std::ofstream file(fileName, std::ios_base::out | std::ios_base::binary);
    if (file.good()) {
        const int w = this->GetWidth();
        const int h = this->GetHeight();

        Array<u8> data = this->UnpackToRGBA(0, true);

        u8 tgaHdr[18] = { 0 };
        tgaHdr[2] = 2;      // Uncompressed RGB
        tgaHdr[12] = w & 0xff;
        tgaHdr[13] = (w >> 8) & 0xff;
        tgaHdr[14] = h & 0xff;
        tgaHdr[15] = (h >> 8) & 0xff;
        tgaHdr[16] = 32;    // RGB + Alpha
        file.write(reinterpret_cast<const char*>(&tgaHdr[0]), sizeof(tgaHdr));

        // since not all image viewers care about image orientation flags
        // I'll flip the image vertically
        const int lineLength = w * 4;
        u8* ptr = data.data();
        for (int y = h - 1; y >= 0; --y) {
            u8* line = ptr + (y * lineLength);
            file.write(reinterpret_cast<const char*>(line), lineLength);
        }

        file.flush();

        result = true;
    }

    return result;
}


#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_FOURCC      0x00000004
#define DDPF_RGB         0x00000040

#define DDSD_CAPS        0x00000001
#define DDSD_HEIGHT      0x00000002
#define DDSD_WIDTH       0x00000004
#define DDSD_PITCH       0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE  0x00080000
#define DDSD_DEPTH       0x00800000

#define DDSCAPS_COMPLEX  0x00000008
#define DDSCAPS_TEXTURE  0x00001000
#define DDSCAPS_MIPMAP   0x00400000

#define DDSCAPS2_CUBEMAP 0x00000200
#define DDSCAPS2_VOLUME  0x00200000

#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000
#define DDSCAPS2_CUBEMAP_ALL_FACES (DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX | DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY | DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ)

struct DDSHeader {
    u32 dwMagic;
    u32 dwSize;
    u32 dwFlags;
    u32 dwHeight;
    u32 dwWidth;
    u32 dwPitchOrLinearSize;
    u32 dwDepth;
    u32 dwMipMapCount;
    u32 dwReserved[11];

    struct {
        u32 dwSize;
        u32 dwFlags;
        u32 dwFourCC;
        u32 dwRGBBitCount;
        u32 dwRBitMask;
        u32 dwGBitMask;
        u32 dwBBitMask;
        u32 dwRGBAlphaBitMask;
    } ddpfPixelFormat;

    struct {
        u32 dwCaps1;
        u32 dwCaps2;
        u32 Reserved[2];
    } ddsCaps;

    u32 dwReserved2;
};

bool PCTTexture::ExportToDDS(const String& fileName) {
    bool result = false;

    std::ofstream file(fileName, std::ios_base::out | std::ios_base::binary);
    if (file.good()) {
        const int w = this->GetWidth();
        const int h = this->GetHeight();

        const int numMips = this->GetMipsCount();
        const int depth = this->IsCubemap() ? 0 : 1;

        DDSHeader header;
        std::memset(&header, 0, sizeof(header));

        header.dwMagic = 0x20534444; // 'DDS '
        header.dwSize = sizeof(DDSHeader) - sizeof(header.dwMagic); // do not include the magic
        header.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | (numMips > 1 ? DDSD_MIPMAPCOUNT : 0) | (depth > 1 ? DDSD_DEPTH : 0);
        header.dwHeight = h;
        header.dwWidth = w;
        header.dwPitchOrLinearSize = 0;
        header.dwDepth = (depth > 1) ? depth : 0;
        header.dwMipMapCount = (numMips > 1) ? numMips : 0;

        header.ddpfPixelFormat.dwSize = sizeof(header.ddpfPixelFormat);
        header.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        header.ddpfPixelFormat.dwRGBBitCount = 32;
        header.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
        header.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
        header.ddpfPixelFormat.dwBBitMask = 0x000000FF;
        header.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;

        header.ddsCaps.dwCaps1 = DDSCAPS_TEXTURE | (numMips > 1 ? DDSCAPS_MIPMAP | DDSCAPS_COMPLEX : 0) | (depth != 1 ? DDSCAPS_COMPLEX : 0);
        header.ddsCaps.dwCaps2 = (depth > 1) ? DDSCAPS2_VOLUME : (depth == 0) ? DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES : 0;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        if (this->IsCubemap()) {
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < numMips; ++j) {
                    const int pictureIdx = i + (j * 6);
                    Array<u8> data = this->UnpackToRGBA(pictureIdx, true);
                    file.write(reinterpret_cast<const char*>(data.data()), data.size());
                }
            }
        } else {
            for (int i = 0; i < numMips; ++i) {
                Array<u8> data = this->UnpackToRGBA(i, true);
                file.write(reinterpret_cast<const char*>(data.data()), data.size());
            }
        }

        file.flush();

        result = true;
    }

    return result;
}

bool PCTTexture::ExportToHDR(const String& fileName) {
    bool result = false;

    std::ofstream file(fileName, std::ios_base::out | std::ios_base::binary);
    if (file.good()) {
        const int w = this->GetWidth();
        const int h = this->GetHeight();

        qc::String header = qc::String("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y ") + std::to_string(h) + " +X " + std::to_string(w) + "\n";
        file << header;

        Array<u8> data = this->UnpackToRGBE(0);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.flush();

        result = true;
    }

    return result;
}



} // namespace qc
