#pragma once
// Only verified stock TargetEntityFilter.SelectionType values are named.
// Leave the rest to the native engine rather than assigning guessed labels.
namespace CustomComponents::Engine::TargetSelectionTypes {
enum class Type : int {Manual=1,CardDatabase=5};
inline constexpr int ToNative(Type type){return static_cast<int>(type);}
inline constexpr bool Is(int native,Type expected){return native==ToNative(expected);}
}
