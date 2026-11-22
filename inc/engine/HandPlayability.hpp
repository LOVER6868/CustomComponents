#pragma once
#include <optional>
#include <span>
#include "engine/FrontendServices.hpp"
#include "engine/EntityAccess.hpp"
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
namespace CustomComponents::Engine::HandPlayability {
using Obj=FrontendServices::Obj;
inline std::shared_ptr<Core::ManagedRuntime::Root> cacheRoot,gameRoot;
inline std::shared_ptr<Core::ManagedRuntime::Root> highlightRoot,highlightGame;
inline void(*originalHighlight)(Obj*,const void*)=nullptr;
inline Core::Observers<Obj*> beforeHighlight,afterHighlight;
static void Highlight(Obj* command,const void* method){
    static auto owner=BNM::Class("PvZCards.Game","HighlightPlayableCardsCommand").GetField("<GameModel>k__BackingField").cast<Obj*>();
    Obj* game=owner.IsValid()?Core::Metadata::Bind(owner,command).Get():nullptr;
    if(game && game==FrontendServices::Game()){
        highlightRoot=std::make_shared<Core::ManagedRuntime::Root>(command,true);
        highlightGame=std::make_shared<Core::ManagedRuntime::Root>(game,true);
    }
    beforeHighlight.Notify(command);
    originalHighlight(command,method);
    afterHighlight.Notify(command);
}
inline bool dirty=false;
inline thread_local Obj* cacheGame=nullptr;
inline void(*originalCacheCommand)(Obj*,const void*)=nullptr;
inline Obj*(*originalGetPlays)(Obj*,int,const void*)=nullptr;
inline Core::Observers<int> beforeRefresh,afterRefresh;
inline Core::Observers<Obj*,int,Obj*> afterValidPlays;
inline void Capture(Obj* cache,Obj* game){if(!cache || !game || game!=FrontendServices::Game())return;if(cacheRoot && gameRoot && cacheRoot->Get()==cache && gameRoot->Get()==game)return;cacheRoot=std::make_shared<Core::ManagedRuntime::Root>(cache,true);gameRoot=std::make_shared<Core::ManagedRuntime::Root>(game,true);}
inline Obj* Cache(){Obj* game=FrontendServices::Game();if(!game)return nullptr;if(gameRoot && gameRoot->Get()==game && cacheRoot && cacheRoot->Get())return cacheRoot->Get();static auto type=BNM::Class("PvZCards.Game","PlayCache");Obj* cache=FrontendServices::Provide(type,game);Capture(cache,game);return cache;}
inline int InputState(){Obj* holder=FrontendServices::Holder();static auto metadata=BNM::Class("PvZCards.Game","GameInputStateHolder").GetField("GameInputState").cast<int>();auto field=metadata;return holder && field.IsValid()?field[holder].Get():-1;}
inline bool DisplayAllowed(int state){return state>=3 && state<=9 && ((0x67u>>(state-3))&1u);}
inline std::optional<bool> Playable(int card){
    Obj* cache=Cache();const int state=InputState();if(!cache || state<0)return std::nullopt;
    if(dirty || !DisplayAllowed(state))return false;
    static auto metadata=BNM::Class("PvZCards.Game","PlayCache").GetField("PlayableCards").cast<BNM::Structures::Mono::List<int>*>();auto field=metadata;
    auto* list=field.IsValid()?field[cache].Get():nullptr;if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
    for(int i=0;i<list->size;++i)if(list->items->m_Items[i]==card)return true;return false;
}
static Obj* GetPlays(Obj* facade,int player,const void* method){
    if(cacheGame)FrontendServices::ObserveFacade(facade,cacheGame);
    Obj* result=originalGetPlays(facade,player,method);
    if(!cacheGame && result)afterValidPlays.Notify(facade,player,result);
    return result;
}
static void CacheCommand(Obj* command,const void* method){
    static auto type=BNM::Class("PvZCards.Game","CachePlaysCommand");
    static auto gameMetadata=type.GetField("<GameModel>k__BackingField").cast<Obj*>();static auto cacheMetadata=type.GetField("<PlayCache>k__BackingField").cast<Obj*>();
    auto gameField=gameMetadata;auto cacheField=cacheMetadata;Obj* game=gameField[command].Get();Obj* cache=cacheField[command].Get();
    struct Restore{Obj* previous=cacheGame;~Restore(){cacheGame=previous;}}restore;cacheGame=game;Capture(cache,game);
    originalCacheCommand(command,method);if(game==FrontendServices::Game())dirty=false;
}
// Mirrors CachePlaysCommand's public API calls. CachePlays takes ownership of
// the pooled Play list; do not return that list after passing it to the cache.
inline bool Refresh(std::span<const int> cards){
    Obj* game=FrontendServices::Game();Obj* cache=Cache();if(cards.empty())return true;
    if(!game || !cache)return false;
    const bool wasDirty=dirty;
    struct RestoreDirty {bool previous;bool success=false;~RestoreDirty(){if(!success)dirty=previous;}} restoreDirty{wasDirty};
    dirty=true;
    for(int card:cards)beforeRefresh.Notify(card);
    static auto gameType=BNM::Class("PvZCards.Core.Utilities","GameModel");
    static auto deciderMetadata=gameType.GetField("<GameFacadeDecider>k__BackingField").cast<Obj*>();static auto playerMetadata=gameType.GetField("LocalPlayerId").cast<int>();
    static auto cacheMetadata=BNM::Class("PvZCards.Game","PlayCache").GetMethod("CachePlays",1).cast<void>();
    static auto playsMetadata=BNM::Class("PvZCards.Core.Utilities","GameFacade").GetMethod("GetValidPlays",1).cast<Obj*>();
    auto deciderField=deciderMetadata;auto player=playerMetadata;auto store=cacheMetadata;auto get=playsMetadata;
    Obj* decider=deciderField[game].Get();if(!decider || !player.IsValid() || !store.IsValid() || !get.IsValid())return false;
    auto facadeMethod=BNM::Class(decider).GetMethod("GetGameFacade",0).cast<Obj*>();if(!facadeMethod.IsValid())return false;
    bool complete=false;
    const auto exception=BNM::TryInvoke([&]{
        struct Restore{Obj* previous=cacheGame;~Restore(){cacheGame=previous;}}restore;cacheGame=game;
        Obj* facade=facadeMethod[decider]();if(!facade)return;
        FrontendServices::ObserveFacade(facade,game);
        Obj* plays=get[facade](player[game].Get());if(!plays)return;store[cache](plays);dirty=false;complete=true;
    });
    if(exception.IsValid() || !complete)return false;
    restoreDirty.success=true;
    // Updating PlayCache alone leaves the old glowing/dimmed hand state.
    // Reuse the native highlighter and its input-state/turn restrictions.
    static auto update=BNM::Class("PvZCards.Game","HighlightPlayableCardsCommand").GetMethod("UpdateCardsHighlightState",1).cast<void>();
    Obj* highlighter=highlightRoot && highlightGame && highlightGame->Get()==game?highlightRoot->Get():nullptr;
    if(highlighter && update.IsValid()){
        auto* ids=BNM::Class("System","Int32").NewArray<int>(cards.size());
        Core::ManagedRuntime::Root root(reinterpret_cast<Obj*>(ids));
        if(root.Get()){
            std::copy(cards.begin(),cards.end(),ids->m_Items);
            const auto error=BNM::TryInvoke([&]{Core::Metadata::Bind(update,highlighter)(ids);});
            if(error.IsValid())return false;
        }
    }else return false;
    for(int card:cards)afterRefresh.Notify(card);
    return true;
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Game","HighlightPlayableCardsCommand").GetMethod("Execute",0),Highlight,originalHighlight) && Core::Hooks::Install(BNM::Class("PvZCards.Game","CachePlaysCommand").GetMethod("Execute",0),CacheCommand,originalCacheCommand) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","GameFacade").GetMethod("GetValidPlays",1),GetPlays,originalGetPlays);}
}
