
// Included from the swap implementation, which owns Slot and lane placement.
namespace Random {
using String=BNM::Structures::Mono::String;
struct Runtime:Obj {
    std::uint8_t inherited[0x40]{};
    String* Targets=nullptr;
    BNM_CustomClass(Runtime,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","SwapRandomlyEffect","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveCardToLanesEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(Targets,BNM::Defaults::Get<String*>(),"Targets");
    void SetTargets(String* text){
        if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&Targets),text);
        else Targets=text;
    }
    void Construct(){Core::NativeConstruction::Base(this);Targets=nullptr;}
    void DeepCopyFrom(Obj* from,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,from,instances);SetTargets(from?reinterpret_cast<Runtime*>(from)->Targets:nullptr);}
    void HandleReturnToPool(Obj* instances){SetTargets(nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Descriptor:Obj {
    BNM_CustomClass(Descriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SwapRandomlyEffectDescriptor","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor","EngineLib.dll").Build(),{});
    void Construct(){Core::NativeConstruction::Base(this);}
    Obj* ToEffect(Obj* instances,Obj*){return Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SwapRandomlyEffect"));}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
inline bool Is(Obj* effect,const char* name){
    return effect && effect->klass && effect->klass->name && std::string_view(effect->klass->name)==name;
}
inline bool Convert(Obj* descriptor,Obj* instances,Obj*,Obj*& out){
    if(!Is(descriptor,"SwapRandomlyEffectDescriptor"))return false;
    out=reinterpret_cast<Descriptor*>(descriptor)->ToEffect(instances,nullptr);
    return out!=nullptr;
}
inline void Collect(Obj* entity,Obj*,Obj* result){
    auto* effects=reinterpret_cast<Engine::EntityAccess::List*>(result);
    if(!Engine::EntityAccess::Valid(effects))return;
    std::vector<int> indices,targets;
    for(int i=0;i<effects->size;++i){
        Obj* effect=effects->items->m_Items[i];
        if(!Is(effect,"SwapRandomlyEffect"))continue;
        indices.push_back(i);
        const int target=Variables::Field(effect,"TargetId");
        if(target>=0 && std::find(targets.begin(),targets.end(),target)==targets.end())targets.push_back(target);
    }
    if(indices.size()<2)return;
    auto remove=BNM::Class(result).GetMethod("RemoveAt",1).cast<void>();
    Obj* instances=Engine::Match::Instances(entity);
    if(!remove.IsValid() || !instances)return;
    std::string ids;
    for(int id:targets){if(!ids.empty())ids+=',';ids+=std::to_string(id);}
    reinterpret_cast<Runtime*>(effects->items->m_Items[indices.front()])->SetTargets(BNM::CreateMonoString(ids));
    for(auto i=indices.rbegin();i!=indices.rend()-1;++i){
        Obj* effect=effects->items->m_Items[*i];
        remove[result](*i);Core::ManagedPool::Return(instances,effect);
    }
}
inline std::vector<Slot> Selected(Obj* model,String* ids){
    std::vector<Slot> slots;
    if(!ids)return slots;
    const std::string values=ids->str();
    std::string_view rest(values);
    while(!rest.empty()){
        const auto comma=rest.find(',');
        const auto item=rest.substr(0,comma);
        int id=-1;
        const auto parsed=std::from_chars(item.data(),item.data()+item.size(),id);
        if(parsed.ec!=std::errc{} || parsed.ptr!=item.data()+item.size())return {};
        auto slot=Capture(model,id);
        auto* card=Engine::EntityAccess::Find(model,id);
        if(!slot || !Engine::FighterPair::Live(model,id) || Engine::EntityAccess::Component(card,"FaceDown"))return {};
        slots.push_back(*slot);
        if(comma==rest.npos)break;
        rest.remove_prefix(comma+1);
    }
    return slots;
}
struct Fighter {Engine::BoardPlacementPolicy::Card traits;};
struct Destination {int residents=0;bool teamup=false,water=false;};
inline std::vector<int> Plan(Obj* model,const std::vector<Slot>& slots){
    const int count=static_cast<int>(slots.size());
    std::vector<Fighter> fighters;
    std::map<std::pair<int,int>,Destination> lanes;
    auto get=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1).cast<BNM::Structures::Mono::List<int>*>();
    if(!get.IsValid())return {};
    for(const auto& slot:slots){
        auto* card=Engine::EntityAccess::Find(model,slot.card);
        fighters.push_back({{true,false,Engine::EntityAccess::Component(card,"Plants")!=nullptr,
            Engine::BoardPlacement::Active(card,"Teamup"),Engine::BoardPlacement::Active(card,"Aquatic")}});
        auto [entry,inserted]=lanes.try_emplace({slot.lane,slot.player});
        if(!inserted)continue;
        auto* lane=Engine::EntityAccess::Find(model,slot.lane);
        auto* occupants=get[Engine::EntityAccess::Component(lane,"Lane")](slot.player);
        if(!Engine::EntityAccess::Valid(occupants))return {};
        auto& state=entry->second;
        state.water=Engine::EntityAccess::Component(lane,"WaterTerrain")!=nullptr;
        for(int i=0;i<occupants->size;++i){
            const int id=occupants->items->m_Items[i];
            if(std::any_of(slots.begin(),slots.end(),[id](const Slot& s){return s.card==id;}))continue;
            ++state.residents;
            state.teamup|=Engine::BoardPlacement::Active(Engine::EntityAccess::Find(model,id),"Teamup");
        }
    }
    std::vector<std::vector<int>> choices(count);
    for(int i=0;i<count;++i){
        for(int j=0;j<count;++j)
            if(slots[i].lane!=slots[j].lane && slots[i].player==slots[j].player)choices[i].push_back(j);
        auto* card=Engine::EntityAccess::Find(model,slots[i].card);
        for(int n=static_cast<int>(choices[i].size());n>1;--n){
            const int roll=Engine::Match::Roll(card,n);
            if(roll<0)return {};
            std::swap(choices[i][n-1],choices[i][roll]);
        }
    }
    std::vector<int> assignment(count,-1);
    std::vector<bool> used(count,false);
    const auto search=[&](auto&& self,int i)->bool{
        if(i==count)return true;
        for(int j:choices[i]){
            if(used[j])continue;
            auto& lane=lanes.at({slots[j].lane,slots[j].player});
            const auto prior=lane;
            if(!Engine::SwapPolicy::Fits(fighters[i].traits,lane.residents,lane.teamup,lane.water,{}))continue;
            used[j]=true;assignment[i]=j;
            ++lane.residents;lane.teamup|=fighters[i].traits.teamup;
            if(self(self,i+1))return true;
            lane=prior;used[j]=false;
        }
        return false;
    };
    return search(search,0)?assignment:std::vector<int>{};
}
inline bool Execute(Obj* model,Obj* effect){
    if(!model)return false;
    const auto slots=Selected(model,reinterpret_cast<Runtime*>(effect)->Targets);
    if(slots.size()<2)return false;
    const auto destinations=Plan(model,slots);
    if(destinations.empty())return false;
    auto* instances=Engine::Match::Instances(Engine::EntityAccess::Find(model,slots.front().card));
    static const auto lane=BNM::Class("PvZCards.Core.Systems.Utilities","LaneModifier");
    auto* modifier=Engine::Match::Provide(instances,lane.GetMonoType());
    auto* history=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","History").GetMonoType());
    static auto remove=lane.GetMethod("RemoveCardFromLane",1).cast<void>();
    static auto place=lane.GetMethod("PlayCardInLane",1).cast<int>();
    if(!modifier || !history || !remove.IsValid() || !place.IsValid())return false;
    std::vector<int> order(slots.size());
    std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](int a,int b){return slots[destinations[a]].order<slots[destinations[b]].order;});
    std::vector<int> removed,added;
    auto error=BNM::TryInvoke([&]{
        for(const auto& slot:slots){remove[modifier](slot.card);removed.push_back(slot.card);}
        for(int i:order){
            const auto& to=slots[destinations[i]];
            place[modifier](LanePlacementData{slots[i].card,to.lane,to.player,to.order});
            added.push_back(slots[i].card);
        }
    });
    History::SwapPayload payload;
    if(!error.IsValid())for(std::size_t i=0;i<slots.size();++i){
        const auto& before=slots[i];
        const auto after=Capture(model,before.card);
        const auto& wanted=slots[destinations[i]];
        if(!after || after->lane!=wanted.lane || after->order!=wanted.order)break;
        payload.moves.push_back({{before.card,before.lane,before.order},{after->card,after->lane,after->order},before.player});
    }
    if(payload.moves.size()!=slots.size()){
        const auto rollback=BNM::TryInvoke([&]{
            for(int id:added)remove[modifier](id);
            std::sort(order.begin(),order.end(),[&](int a,int b){return slots[a].order<slots[b].order;});
            for(int i:order){
                const auto& slot=slots[i];
                if(std::find(removed.begin(),removed.end(),slot.card)!=removed.end())
                    place[modifier](LanePlacementData{slot.card,slot.lane,slot.player,slot.order});
            }
        });
        if(rollback.IsValid())LOG_ERR("CustomComponents/SwapRandomly: native rollback failed");
        return false;
    }
    History::Record record;
    record.kind=History::Kind::SwapTransaction;
    record.stage=History::Stage::Committed;
    record.model=reinterpret_cast<std::uintptr_t>(model);
    record.modelToken=Core::ModelRegistry::Identify(model,true);
    record.history=reinterpret_cast<std::uintptr_t>(history);
    record.stockHistoryAnchor=Engine::CustomRecordHistoryAnchor::Count(history);
    record.visual=record.model==Core::SceneViewContext::ModelIdentity()?History::VisualPolicy::Required:History::VisualPolicy::NoVisual;
    record.entityId=slots.front().card;
    record.ownerId=slots.front().player;
    record.sourceId=Variables::Field(effect,"SourceId");
    record.effectId=Variables::Field(effect,"EffectEntityId");
    record.payload=std::move(payload);
    if(!Core::Performance::aiDepth && !History::Commit(std::move(record)))
        LOG_ERR("CustomComponents/SwapRandomly: record commit failed");
    Engine::SimulationChanges::Changed(model);
    return true;
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    if(!Is(context.effect,"SwapRandomlyEffect"))return true;
    auto* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    Execute(model,context.effect);
    return false;
}
inline bool Initialize(){
    auto desc=BNM::Class("PvZCards.Engine.Components","SwapRandomlyEffectDescriptor");
    auto rt=BNM::Class("PvZCards.Engine.Effects","SwapRandomlyEffect");
    auto baseDesc=BNM::Class("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor");
    auto baseRt=BNM::Class("PvZCards.Engine.Effects","MoveCardToLanesEffect");
    return CustomComponentRegistry::Register({"SwapRandomlyEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","MoveCardToLanesEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(desc,baseDesc) && Core::ComponentLifecycle::Patch(rt,baseRt) &&
        Core::ComponentLifecycle::PatchVirtual(desc,baseDesc,BNM::Class("PvZCards.Engine.Components","EffectDescriptor"),"ToEffect",2) &&
        Engine::AbilityConversion::Add(Convert) && Engine::EffectCreationEvents::created.Add(Collect) &&
        Engine::EffectGuards::RegisterPreparation(Prepare,-27);
}
}

