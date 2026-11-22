#pragma once
#include "queries/InDeckQuery.hpp"
#include "core/NativeConstruction.hpp"
#include "engine/StartingSuperpowers.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EntityAccess.hpp"

namespace CustomComponents::Queries::CardPools {
inline auto Entries(Obj* pool) {
    Obj* ordered=Engine::EntityAccess::Component(pool,"OrderedCardPool");
    return ordered?Core::Metadata::Bind(inDeckOrderedPoolEntries,ordered).Get():nullptr;
}
inline int Guid(Obj* model,Obj* entry) {
    if(!entry)return -1;
    const int guid=Core::Metadata::Bind(inDeckPoolEntryGuid,entry).Get();
    if(guid>0)return guid;
    return Engine::BoardPlacement::Guid(Engine::EntityAccess::Find(model,Core::Metadata::Bind(inDeckPoolEntryEntityId,entry).Get()));
}
inline bool Member(Obj* candidate,Obj* context,bool byGuid) {
    Obj* model=Engine::BoardPlacement::Model(context);
    if(!candidate || !model)return false;
    const int value=byGuid?Engine::BoardPlacement::Guid(candidate):Engine::Match::Id(candidate);
    if(value<0 || (!byGuid && Engine::EntityAccess::Find(model,value)!=candidate))return false;
    auto* entities=Core::Metadata::Bind(inDeckModelEntities,model).Get();
    if(!Engine::EntityAccess::Valid(entities))return false;
    for(int e=0;e<entities->size;++e){
        Obj* pool=entities->items->m_Items[e];
        if(!Engine::EntityAccess::Component(pool,"SuperpowerPool"))continue;
        const auto* entries=Entries(pool);if(!Engine::EntityAccess::Valid(entries))continue;
        for(int i=0;i<entries->size;++i){Obj* entry=entries->items->m_Items[i];
            if(entry && (byGuid?Guid(model,entry):Core::Metadata::Bind(inDeckPoolEntryEntityId,entry).Get())==value)return true;
        }
    }
    return false;
}
inline bool SizeMatches(Obj* query,Obj* candidate,Obj* context,bool powers) {
    Obj* model=Engine::BoardPlacement::Model(context);
    if(!model || !candidate)return false;
    const int owner=Engine::EntityAccess::Component(candidate,"Player")?Engine::Match::Id(candidate):Engine::BoardPlacement::Owner(candidate);
    if(owner<0)return false;
    Obj* pool=Engine::CardPoolAccess::Get(model,owner,powers);
    if(!Engine::EntityAccess::Component(pool,powers?"SuperpowerPool":"Deck"))return false;
    const auto* entries=Entries(pool);if(!Engine::EntityAccess::Valid(entries))return false;
    const int count=entries->size;
    static auto number=BNM::Class("PvZCards.Engine.Queries","CardsInHandQuery").GetField("NumCards").cast<int>();
    static auto comparison=BNM::Class("PvZCards.Engine.Queries","CardsInHandQuery").GetField("ComparisonOperator").cast<int>();
    if(!number.IsValid() || !comparison.IsValid())return false;
    const int expected=Core::Metadata::Bind(number,query).Get();
    switch(Core::Metadata::Bind(comparison,query).Get()){
        case 0:return count==expected;
        case 1:return count<=expected;
        case 2:return count>=expected;
        default:return false;
    }
}
#define CC_POOL_QUERY(Name, BaseName, Expression) \
struct Name : Obj { \
    BNM_CustomClass(Name,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries",#Name,"EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries",#BaseName,"EngineLib.dll").Build(),{}); \
    void Construct(){Core::NativeConstruction::Base(this);} \
    bool Matches(Obj* candidate,Obj* context){return Expression;} \
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor"); \
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build()); \
    BNM_CustomMethodSkipTypeMatch(Matches); \
};
CC_POOL_QUERY(StartingSuperpowersCardGuidQuery,InHandQuery,Engine::StartingSuperpowers::Matches(candidate,context))
CC_POOL_QUERY(InSuperpowerDeckQuery,InHandQuery,Member(candidate,context,false))
CC_POOL_QUERY(InSuperpowerDeckGuidQuery,InHandQuery,Member(candidate,context,true))
CC_POOL_QUERY(SuperpowerDeckSizeQuery,CardsInHandQuery,SizeMatches(this,candidate,context,true))
CC_POOL_QUERY(RegularDeckSizeQuery,CardsInHandQuery,SizeMatches(this,candidate,context,false))
#undef CC_POOL_QUERY
inline bool Initialize() {
    if(!Engine::StartingSuperpowers::Initialize() || !InitializeInDeckMembershipMetadata())return false;
    for(const auto [name,baseName]:{
            std::pair{"StartingSuperpowersCardGuidQuery","InHandQuery"},
            std::pair{"InSuperpowerDeckQuery","InHandQuery"},
            std::pair{"InSuperpowerDeckGuidQuery","InHandQuery"},
            std::pair{"SuperpowerDeckSizeQuery","CardsInHandQuery"},
            std::pair{"RegularDeckSizeQuery","CardsInHandQuery"}}){
        const auto type=BNM::Class("PvZCards.Engine.Queries",name);
        const auto base=BNM::Class("PvZCards.Engine.Queries",baseName);
        // Registry strings must remain valid for the module lifetime.
        static constexpr const char* serialized[]={"PvZCards.Engine.Queries.StartingSuperpowersCardGuidQuery","PvZCards.Engine.Queries.InSuperpowerDeckQuery","PvZCards.Engine.Queries.InSuperpowerDeckGuidQuery","PvZCards.Engine.Queries.SuperpowerDeckSizeQuery","PvZCards.Engine.Queries.RegularDeckSizeQuery"};
        const char* full=nullptr;for(const auto* value:serialized)if(std::string_view(value).substr(std::string_view("PvZCards.Engine.Queries.").size())==name)full=value;
        if(!type.IsValid() || !base.IsValid() || !full || !PatchInDeckQueryDispatch(type,base,name) ||
            !RegisterQuery({name,full,"PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries",baseName}))return false;
    }
    return true;
}
}
