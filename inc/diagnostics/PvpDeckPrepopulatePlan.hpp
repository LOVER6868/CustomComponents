#pragma once
#include <cstdint>
#include <string_view>

// Pure deterministic startup-shape validator used by the PvP prepopulation
// prototype. No game types, pointers, clocks or allocations.
namespace CustomComponents::Diagnostics::PvpDeckPrepopulatePlan {
inline constexpr int PoolIds[4] = {1,2,6,7};
inline constexpr int PoolSizes[4] = {40,4,40,4};
// The authorization format is intentionally backward-compatible. "auto"
// accepts any shape-valid live match hash; a 16-character lowercase hex pin
// restricts the materializer to a specific observed match configuration.
inline bool AcceptFingerprint(std::string_view configured, std::uint64_t actual) {
    if (configured=="auto") return true;
    if (configured.size()!=16) return false;
    static constexpr char kHex[]="0123456789abcdef";
    for (int n=15; n>=0; --n) {
        if (configured[n]!=kHex[static_cast<unsigned>(actual & 15ull)]) return false;
        actual >>= 4;
    }
    return true;
}

struct Builder {
    std::uint64_t fingerprint = 14695981039346656037ull;
    int nextPool = 0;
    int nextEntry = 0;
    int total = 0;
    bool good = true;
    bool poolOpen = false;

    void Mix(std::uint32_t value) {
        for (unsigned j=0;j<4;j++) {
            fingerprint ^= (value >> (j * 8)) & 255u;
            fingerprint *= 1099511628211ull;
        }
    }
    bool BeginPool(int id, int count) {
        if (!good || nextPool>=4 || id!=PoolIds[nextPool] ||
            count!=PoolSizes[nextPool] || nextEntry!=0 || poolOpen) return good=false;
        poolOpen=true;
        Mix(static_cast<std::uint32_t>(id));
        Mix(static_cast<std::uint32_t>(count));
        return true;
    }
    bool AddEntry(int guid, int entityId) {
        if (!good || !poolOpen || nextPool>=4 || nextEntry>=PoolSizes[nextPool] ||
            guid<=0 || entityId!=-1) return good=false;
        Mix(static_cast<std::uint32_t>(guid));
        ++total;
        if (++nextEntry==PoolSizes[nextPool]) {
            poolOpen=false;
            ++nextPool;
            nextEntry=0;
        }
        return true;
    }
    bool Complete() const { return good && !poolOpen && nextPool==4 && nextEntry==0 && total==88; }
};
} // namespace CustomComponents::Diagnostics::PvpDeckPrepopulatePlan
