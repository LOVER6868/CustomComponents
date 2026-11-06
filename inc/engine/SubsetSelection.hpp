#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include <cstring>
#include "core/Observers.hpp"
#include "engine/PlacementQueryContext.hpp"
#include "engine/EntityAccess.hpp"
#include "core/CandidateRules.hpp"
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Engine::SubsetSelection {
using Obj=BNM::IL2CPP::Il2CppObject;
enum class Purpose { Database, DeckDraw };
struct Context {
    Obj* finder=nullptr;Obj* model=nullptr;Obj* query=nullptr;Obj* effect=nullptr;Obj* deck=nullptr;
    Purpose purpose=Purpose::Database;
    int result=-1;bool returned=false;
};
inline thread_local const Context* current=nullptr;
// The creation extension supplies this capability; the stock finder owner
// remains shared with draw/creation provenance and preserves authored queries.
inline bool(*hasAquaticCreationGrant)(Obj*)=nullptr;
inline Core::Observers<const Context&> beforeFind,afterFind;
inline Core::CandidateRules<Context> candidateRules;
inline Context Request(Obj* finder,Purpose purpose=Purpose::Database,Obj* deck=nullptr) {
    Context context;
    if(current && current->finder==finder) context=*current;
    else {
        context.finder=finder;
        static auto model=BNM::Class("PvZCards.Core.Systems.Utilities","CardFromSubsetFinder")
            .GetField("<Model>k__BackingField").cast<Obj*>();
        auto field=model;
        context.model=finder && field.IsValid()?field[finder].Get():nullptr;
    }
    context.purpose=purpose;context.deck=deck;return context;
}
inline Obj* QueryEffect(Obj* model,Obj* effect) {
    static auto field=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();auto f=field;
    return model && effect && f.IsValid()?EntityAccess::Find(model,f[effect].Get()):nullptr;
}
struct Scope {
    Context context;const Context* previous;
    PlacementQueryContext::Scope placement;
    Scope(Obj* finder,Obj* query,Obj* effect):context(Request(finder)),previous(current),placement({context.model,QueryEffect(context.model,effect),nullptr,-1}) {
        context.query=query;context.effect=effect;context.result=-1;context.returned=false;
        current=&context;
        try {beforeFind.Notify(context);} catch(...) {current=previous;throw;}
    }
    int Finish(int result){context.result=result;context.returned=true;return result;}
    ~Scope(){current=previous;afterFind.Notify(context);}
};
template<class Original,class Guid>
int PickDatabase(Obj* finder,Obj* cards,const void* method,Original original,Guid guidOf) {
    auto rules=candidateRules.Read();
    if(rules->empty()) return original(finder,cards,method);
    if(!cards) return -1;
    auto* klass=BNM::Class(cards).GetClass();
    if(!klass || !klass->name || std::strcmp(klass->name,"List`1")!=0) return -1;
    auto context=Request(finder);
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(cards);
    if(!cards || !list->items || list->size<0 || list->size>int(list->items->capacity)) return -1;
    std::vector<Obj*> allowed;
    for(int i=0;i<list->size;++i) {
        auto* card=list->items->m_Items[i];
        if(Core::CandidateRules<Context>::Allows(rules,context,guidOf(card))) allowed.push_back(card);
    }
    if(allowed.empty()) return -1;
    if(allowed.size()==std::size_t(list->size)) return original(finder,cards,method);
    auto type=BNM::Class(cards);
    auto constructor=type.GetMethod(".ctor",0).cast<void>();auto add=type.GetMethod("Add",1).cast<void>();
    if(!constructor.IsValid() || !add.IsValid()) return -1;
    Obj* filtered=type.CreateNewInstance();Core::ManagedRuntime::Root lifetime(filtered);
    if(!lifetime.Get()) return -1;
    const auto error=BNM::TryInvoke([&]{constructor[filtered]();for(auto* card:allowed)add[filtered](card);});
    return error.IsValid()?-1:original(finder,filtered,method);
}
}
