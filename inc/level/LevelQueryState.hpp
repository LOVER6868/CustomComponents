#pragma once
#include <optional>
#include <BNM/Il2CppHeaders.hpp>
namespace CustomComponents::Level::QueryState {
using Obj=BNM::IL2CPP::Il2CppObject;
// A private hand preview is a query projection, never a simulation mutation.
// Zero means the preview is at base; nullopt means read committed state.
inline std::optional<int>(*previewValue)(Obj*)=nullptr;
}
