#pragma once

#include "qccommon.h"

namespace qc {

class TPLResource {
public:
    TPLResource();
    ~TPLResource();

    bool            LoadFromData(const u8* data, u64 length);
    bool            LoadFromFile(const String& fileName);

private:
    String          mTPLResName;
    String          mTPLResDesc;
};

} // namespace qc
