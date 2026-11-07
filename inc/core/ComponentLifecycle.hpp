#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
namespace CustomComponents::Core::ComponentLifecycle {
inline bool Patch(BNM::Class type,BNM::Class base) {
    auto* klass=type.GetClass();if(!klass)return false;
    for(const char* name:{"DeepCopyFrom","HandleReturnToPool"}) {
        int count=std::string_view(name)=="DeepCopyFrom"?2:1;auto method=type.GetMethod(name,count);auto inherited=base.GetMethod(name,count);
        auto iface=BNM::Class("PvZCards.Engine",count==2?"DeepCopyable":"Poolable");auto entry=iface.GetMethod(name,count);
        if(!method.IsValid() || !inherited.IsValid() || !entry.IsValid())return false;int slot=-1;
        for(int i=0;i<klass->interface_offsets_count;++i)if(klass->interfaceOffsets[i].interfaceType==iface.GetClass())slot=klass->interfaceOffsets[i].offset+entry.GetInfo()->slot;
        int concrete=inherited.GetInfo()->slot;if(slot<0 || slot>=klass->vtable_count || concrete>=klass->vtable_count)return false;
        method.GetInfo()->slot=concrete;for(int index:{slot,concrete})klass->vtable[index]={method.GetInfo()->methodPointer,method.GetInfo()};
    }return true;
}
}
