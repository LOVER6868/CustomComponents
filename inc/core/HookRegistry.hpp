#pragma once
#if defined(__ANDROID__)
#include <dobby.h>
#else
// Host regressions inject this boundary; they do not patch host process code.
extern "C" int DobbyHook(void*,void*,void**);
#endif
#include <mutex>
#include <string>
#include <vector>
#include "log.h"

namespace CustomComponents::Core::Hooks {
struct Entry { void* address; void* replacement; void* original; std::string signature;
    const char* ownerFile = "unknown"; int ownerLine = 0; };
inline std::recursive_mutex lock;
inline std::vector<Entry> entries;
// Clang/GCC evaluate these builtins at the caller (unlike __FILE__/__LINE__
// inside an inline function), so conflicts identify BOTH installation sites.
#if defined(__clang__) || defined(__GNUC__)
#define CC_HOOK_CALLER_FILE __builtin_FILE()
#define CC_HOOK_CALLER_LINE __builtin_LINE()
#else
#define CC_HOOK_CALLER_FILE "unknown"
#define CC_HOOK_CALLER_LINE 0
#endif
inline int InstallNative(void* address,void* replacement,void** original,
    const char* ownerFile = CC_HOOK_CALLER_FILE, int ownerLine = CC_HOOK_CALLER_LINE) {
    if(!address || !replacement || !original) return -1;
    std::lock_guard<std::recursive_mutex> guard(lock);
    for(const auto& entry:entries) if(entry.address==address) {
        if(entry.replacement!=replacement) {
            LOG_ERR("CustomComponents/Hooks: native conflict address=%p existing=%s:%d detour=%p requested=%s:%d detour=%p",
                address,entry.ownerFile,entry.ownerLine,entry.replacement,ownerFile,ownerLine,replacement);
            return -1;
        }
        *original=entry.original;return 0;
    }
    void* trampoline=nullptr;
    const int status=DobbyHook(address,replacement,&trampoline);
    if(status || !trampoline) return status?status:-1;
    *original=trampoline;entries.push_back({address,replacement,trampoline,"native",ownerFile,ownerLine});return 0;
}
// The registry owns native installation. Features subscribe to the owning
// adapter; a second replacement is a conflict, never an implicit hook chain.
template<class Fn> bool InstallAddress(void* address,Fn replacement,Fn& original,
    const char* ownerFile = CC_HOOK_CALLER_FILE, int ownerLine = CC_HOOK_CALLER_LINE) {
    if(!address || !replacement) return false;
    std::lock_guard<std::recursive_mutex> guard(lock);
    const auto target=reinterpret_cast<void*>(replacement);
#if defined(_MSC_VER)
    const auto signature=std::string(__FUNCSIG__);
#else
    const auto signature=std::string(__PRETTY_FUNCTION__);
#endif
    for(const auto& entry:entries) if(entry.address==address) {
        if(entry.replacement!=target || entry.signature!=signature) {
            LOG_ERR("CustomComponents/Hooks: conflict address=%p existing=%s:%d replacement=%p requested=%s:%d replacement=%p",
                address,entry.ownerFile,entry.ownerLine,entry.replacement,ownerFile,ownerLine,target);
            LOG_ERR("CustomComponents/Hooks: existing signature=%s",entry.signature.c_str());
            LOG_ERR("CustomComponents/Hooks: requested signature=%s",signature.c_str());
            return false;
        }
        original=reinterpret_cast<Fn>(entry.original);return original!=nullptr;
    }
    void* trampoline=nullptr;
    const int status=DobbyHook(address,target,&trampoline);
    if(status || !trampoline) {
        LOG_ERR("CustomComponents/Hooks: install failed address=%p status=%d",address,status);
        return false;
    }
    original=reinterpret_cast<Fn>(trampoline);
    entries.push_back({address,target,trampoline,signature,ownerFile,ownerLine});return true;
}
template<class Method,class Fn> bool Install(const Method& method,Fn replacement,Fn& original,
    const char* ownerFile = CC_HOOK_CALLER_FILE, int ownerLine = CC_HOOK_CALLER_LINE) {
    if(!method.IsValid()) return false;
    return InstallAddress(reinterpret_cast<void*>(method.GetOffset()),replacement,original,ownerFile,ownerLine);
}
}
