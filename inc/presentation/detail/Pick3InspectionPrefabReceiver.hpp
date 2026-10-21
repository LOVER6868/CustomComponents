#pragma once
// One-shot typed prefab callback. The completed prefab is consumed on the
// frontend tick, never while ResourceLoader invokes our managed delegate.
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Delegates.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Presentation::Pick3InspectionPrefabReceiver {
    using Obj=BNM::IL2CPP::Il2CppObject;
    inline void (*deliver)(Obj* prefab)=nullptr;
    struct Receiver : Obj {
        BNM_CustomClass(
            Receiver,
            BNM::CompileTimeClassBuilder("Sprig.Pick3","InspectionPrefabReceiver","Assembly-CSharp.dll").Build(),
            BNM::CompileTimeClassBuilder("System","Object","mscorlib.dll").Build(),
            {}
        );
        void OnLoaded(Obj* prefab){if(deliver)deliver(prefab);}
        BNM_CustomMethod(OnLoaded,false,BNM::Defaults::Get<void>(),"OnLoaded",
            BNM::CompileTimeClassBuilder("UnityEngine","GameObject","UnityEngine.CoreModule.dll").Build());
    };
}
