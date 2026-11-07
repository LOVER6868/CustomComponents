#pragma once
namespace CustomComponents::Core::Validation {
// A custom requirement may reject a native success, never rescue a rejection.
template<class Requirement> bool AfterNative(bool valid,Requirement requirement){return valid && requirement();}
}
