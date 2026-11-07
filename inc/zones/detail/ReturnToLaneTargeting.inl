// Included inside DiscardPileZoneEffectsSystem after its physical pair checker.
// Stock FindValidPrimaryTargets removes candidates via IsValidMove. A return
// starts in the discard pile, so that inner move test needs transfer legality.
// Keep stock query evaluation, secondary locations, scoping and pool ownership.
namespace Targeting {
using FindFn=Obj*(*)(Obj*,Obj*,Obj*,const void*);
using MoveFn=bool(*)(Obj*,int,int,Obj*,const void*);
inline FindFn originalPrimary=nullptr,originalSecondary=nullptr;
inline MoveFn originalMove=nullptr;
inline BNM::Field<Obj*> queryModel;
inline BNM::Field<Obj*> finderEntities,entitiesModel;
inline BNM::Field<int> ownerSource;
inline thread_local bool activeDeckSource=false;
struct SourceScope {bool previous=activeDeckSource;explicit SourceScope(Obj* effect){activeDeckSource=FindComponent(effect,"PlayFromDeckToLaneEffectDescriptor")!=nullptr;}~SourceScope(){activeDeckSource=previous;}};
inline ReturnToLaneTargeting::Context Resolve(Obj* finder,Obj* effect,Obj* context) {
    if(!finder || !effect || !context || !queryModel.IsValid() || !ownerSource.IsValid() ||
       (!FindComponent(effect,"ReturnFromDiscardPileToLaneEffectDescriptor") && !FindComponent(effect,"PlayFromDeckToLaneEffectDescriptor"))) return {};
    Obj* model=queryModel[context].Get();
    auto finderField=finderEntities;
    auto modelField=entitiesModel;
    Obj* entities=finderField[finder].Get();
    if(!model || !entities || modelField[entities].Get()!=model ||
       FindEntity(model,entityId[effect].Get())!=effect)return {};
    Obj* owner=FindComponent(effect,"EffectEntity");
    Obj* source=model && owner?FindEntity(model,ownerSource[owner].Get()):nullptr;
    if(!source)return {};
    auto player=getPlayerIdFromCard;
    return {finder,model,player[static_cast<Obj*>(nullptr)](source)};
}
static Obj* Primary(Obj* finder,Obj* effect,Obj* context,const void* method) {
    SourceScope source(effect);
    ReturnToLaneTargeting::Scope scope(Resolve(finder,effect,context));
    return originalPrimary(finder,effect,context,method);
}
static Obj* Secondary(Obj* finder,Obj* effect,Obj* context,const void* method) {
    SourceScope source(effect);
    ReturnToLaneTargeting::Scope scope(Resolve(finder,effect,context));
    return originalSecondary(finder,effect,context,method);
}
static bool Move(Obj* finder,int card,int lane,Obj* validator,const void* method) {
    if(!ReturnToLaneTargeting::Owns(finder))return originalMove(finder,card,lane,validator,method);
    const auto scope=ReturnToLaneTargeting::active;
    Obj* model=static_cast<Obj*>(scope.model);
    if(activeDeckSource)return CheckDeckLanePair(model,card,lane,scope.player);
    const auto pair=CheckDiscardRevivePair(model,card,lane,scope.player);
    static thread_local unsigned samples=0;
    if(++samples<=96)LOG_INFO("CustomComponents/ReturnToLane/TARGET_PAIR: card=%d lane=%d player=%d legal=%d reason=%s source=discard-transfer stockQueries=preserved",
        card,lane,scope.player,int(pair.legal),pair.reason);
    return pair.legal;
}
inline bool Initialize() {
    auto type=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder");
    auto entity=BNM::Class("PvZCards.Engine","Entity");
    auto context=BNM::Class("PvZCards.Engine","EntityQueryContext");
    queryModel=context.GetField("<Model>k__BackingField").cast<Obj*>();
    finderEntities=type.GetField("<EntityFinder>k__BackingField").cast<Obj*>();
    entitiesModel=BNM::Class("PvZCards.Engine","EntityFinder").GetField("Model").cast<Obj*>();
    ownerSource=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    auto primary=type.GetMethod("FindValidPrimaryTargets",2);
    auto secondary=type.GetMethod("FindValidSecondaryTargets",{entity.GetCompileTimeClass(),context.GetCompileTimeClass()});
    auto move=type.GetMethod("IsValidMove",3);
    if(!queryModel.IsValid() || !finderEntities.IsValid() || !entitiesModel.IsValid() ||
       !ownerSource.IsValid() || !entityId.IsValid() || !primary.IsValid() || !secondary.IsValid() || !move.IsValid())return false;
    const bool ready=Core::Hooks::Install(move,Move,originalMove) &&
        Core::Hooks::Install(primary,Primary,originalPrimary) &&
        Core::Hooks::Install(secondary,Secondary,originalSecondary);
    LOG_INFO("CustomComponents/ReturnToLane/TARGETING: ready=%d primaryQuery=stock secondaryQuery=stock legality=discard-transfer",int(ready));
    return ready;
}
}
