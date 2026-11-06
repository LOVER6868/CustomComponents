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
struct Entry { void* address; void* replacement; void* original; std::string signature; };
inline std::recursive_mutex lock;
inline std::vector<Entry> entries;
inline int InstallNative(void* address,void* replacement,void** original) {
    if(!address || !replacement || !original) return -1;
    std::lock_guard<std::recursive_mutex> guard(lock);
    for(const auto& entry:entries) if(entry.address==address) {
        if(entry.replacement!=replacement) return -1;
        *original=entry.original;return 0;
    }
    void* trampoline=nullptr;
    const int status=DobbyHook(address,replacement,&trampoline);
    if(status || !trampoline) return status?status:-1;
    *original=trampoline;entries.push_back({address,replacement,trampoline,"native"});return 0;
}
// The registry owns native installation. Features subscribe to the owning
// adapter; a second replacement is a conflict, never an implicit hook chain.
template<class Fn> bool InstallAddress(void* address,Fn replacement,Fn& original) {
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
            LOG_ERR("CustomComponents/Hooks: conflicting owner/signature address=%p",address);
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
    entries.push_back({address,target,trampoline,signature});return true;
}
template<class Method,class Fn> bool Install(const Method& method,Fn replacement,Fn& original) {
    if(!method.IsValid()) return false;
    return InstallAddress(reinterpret_cast<void*>(method.GetOffset()),replacement,original);
}
}
