#pragma once
// Production path: metadata, class rims and stock trait icons always enabled.
// The former per-popup ADB file override is intentionally retired.
namespace CustomComponents::Presentation::Pick3MetadataIsolation {
    inline constexpr int mode = 0;
    inline int ReadMode() { return mode; }
    inline void Trace(const char*, int, int) {}
}
