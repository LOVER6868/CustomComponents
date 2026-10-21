#pragma once
#include <cstddef>
#include <cstdint>
namespace CustomComponents::Presentation::Pick3BundledUnityFS {
    enum class BundleSlot {Circle,Sun,Heart,Attack,Brain,Button,Colors,Count};
    struct BundleBytes {const std::uint8_t* data;std::size_t size;};
    BundleBytes Get(BundleSlot slot);
}
