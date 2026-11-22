#pragma once
// Values confirmed by the existing native play transport hooks. Do not treat
// every service factory enum as this DTO enum unless it uses the same wire type.
namespace CustomComponents::Engine::PlayTypeCodes {
enum class Type : int {Control=0,Surprise=3};
inline constexpr int ToNative(Type type){return static_cast<int>(type);}
inline constexpr bool Is(int native,Type expected){return native==ToNative(expected);}
}
