#pragma once

#include <string>
#include <vector>

namespace qc {

using i8 = signed __int8;
using u8 = unsigned __int8;
using i16 = signed __int16;
using u16 = unsigned __int16;
using i32 = signed __int32;
using u32 = unsigned __int32;
using i64 = signed __int64;
using u64 = unsigned __int64;

using String = std::string;
template <typename T>
using Array = std::vector<T>;


template <i8 a, i8 b, i8 c, i8 d> struct fourcc {
    static const u32 value = static_cast<u32>(d) << 24 |
                             static_cast<u32>(c) << 16 |
                             static_cast<u32>(b) << 8 |
                             static_cast<u32>(a);
};


struct Random64 {
    u64 u, v, w;
    Random64() {
    }
    Random64(u64 j) {
        Init(j);
    }
    inline void Init(const u64 j) {
        v = 4101842887655102017LL;
        w = 1;
        u = j ^ v; Get64();
        v = u; Get64();
        w = v; Get64();
    }
    inline u64 Get64() {
        u = u * 2862933555777941757LL + 7046029254386353087LL;
        v ^= v >> 17; v ^= v << 31; v ^= v >> 8;
        w = 4294957665U * (w & 0xffffffff) + (w >> 32);
        u64 x = u ^ (u << 21); x ^= x >> 35; x ^= x << 4;
        return (x + v) ^ w;
    }
    inline double GetDouble() {
        return 5.42101086242752217E-20 * Get64();
    }
    inline u32 Get32() {
        return static_cast<u32>(Get64());
    }
};

} // namespace qc
