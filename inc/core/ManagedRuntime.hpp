#pragma once
#include <cstdint>
#include <memory>
#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace BNM::IL2CPP { struct Il2CppObject; }

namespace CustomComponents::Core::ManagedRuntime {
using Obj=BNM::IL2CPP::Il2CppObject;
using NewGCHandleFn=std::uint32_t(*)(Obj*,bool);
using GetGCHandleFn=Obj*(*)(std::uint32_t);
using FreeGCHandleFn=void(*)(std::uint32_t);
inline NewGCHandleFn newGCHandle=nullptr,newWeakGCHandle=nullptr;
inline GetGCHandleFn getGCHandle=nullptr;
inline FreeGCHandleFn freeGCHandle=nullptr;
inline void(*writeBarrier)(Obj*,void**,Obj*)=nullptr;
inline bool Initialize() {
    if(newGCHandle && getGCHandle && freeGCHandle && writeBarrier) return true;
#if defined(__ANDROID__)
    auto library=dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);
    if(!library) return false;
    newGCHandle=reinterpret_cast<NewGCHandleFn>(dlsym(library,"il2cpp_gchandle_new"));
    newWeakGCHandle=reinterpret_cast<NewGCHandleFn>(dlsym(library,"il2cpp_gchandle_new_weakref"));
    getGCHandle=reinterpret_cast<GetGCHandleFn>(dlsym(library,"il2cpp_gchandle_get_target"));
    freeGCHandle=reinterpret_cast<FreeGCHandleFn>(dlsym(library,"il2cpp_gchandle_free"));
    writeBarrier=reinterpret_cast<decltype(writeBarrier)>(dlsym(library,"il2cpp_gc_wbarrier_set_field"));
    dlclose(library);
#endif
    return newGCHandle && getGCHandle && freeGCHandle && writeBarrier;
}
class Root {
public:
    explicit Root(Obj* object,bool weak=false) {
        auto create=weak && newWeakGCHandle ? newWeakGCHandle : newGCHandle;
        if(object && create) handle_=create(object,false);
    }
    ~Root(){ if(handle_ && freeGCHandle) freeGCHandle(handle_); }
    Root(const Root&)=delete;
    Root& operator=(const Root&)=delete;
    Obj* Get() const { return handle_ && getGCHandle ? getGCHandle(handle_) : nullptr; }
private:
    std::uint32_t handle_=0;
};
}
