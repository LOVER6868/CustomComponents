#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/ManagedRuntime.hpp"
#include "core/Observers.hpp"
#include "core/ModelRegistry.hpp"
#include "log.h"
namespace CustomComponents::Engine::PlayServices {
inline Core::Observers<Core::ModelRegistry::Token,int> completed;
// ServiceSubscription unconditionally invokes the supplied completion delegates
// after unsubscribing. BaseService's null check does not protect this wrapper.
struct Completion:BNM::IL2CPP::Il2CppObject {
    int status=0,card=-1;Core::ModelRegistry::Token scene=0;
    void Succeeded(){status=1;completed.Notify(scene,card);}
    void Failed(int reason){status=2;LOG_WARN("CustomComponents/Level/HAND_MODE_FAILURE: reason=%d",reason);}
    BNM_CustomClass(Completion,BNM::CompileTimeClassBuilder("CustomComponents.Engine","PlayServiceCompletion","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("System","Object","mscorlib.dll").Build(),{});
    BNM_CustomField(status,BNM::Defaults::Get<int>(),"Status");
    BNM_CustomField(card,BNM::Defaults::Get<int>(),"Card");
    BNM_CustomField(scene,BNM::Defaults::Get<Core::ModelRegistry::Token>(),"Scene");
    BNM_CustomMethod(Succeeded,false,BNM::Defaults::Get<void>(),"Succeeded");
    BNM_CustomMethod(Failed,false,BNM::Defaults::Get<void>(),"Failed",BNM::CompileTimeClassBuilder("PvZCards.Engine","ServiceFailure","EngineLib.dll").Build());
};
}
