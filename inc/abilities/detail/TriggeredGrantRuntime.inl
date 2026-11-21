// Included inside TriggeredGrants; all mutations use native grant/removal paths.
inline void (*originalCreate)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj* (*originalUpdate)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void (*originalRemove)(Obj*,Obj*,const void*)=nullptr;
static void Create(Obj* creator,Obj* card,Obj* counter,const void* method){
    Obj* model=Core::Metadata::Bind(creatorModel,creator).Get();
    auto* ids=counter?Core::Metadata::Bind(entityIds,counter).Get():nullptr;
    if(Engine::EntityAccess::Valid(ids)){
        for(int i=0;i<ids->size;++i){
            Obj* entity=Engine::EntityAccess::Find(model,ids->items->m_Items[i]);
            Obj* owner=Engine::EntityAccess::Component(entity,"EffectEntity");
            if(owner && Core::Metadata::Bind(abilityOwner,owner).Get()==Engine::Match::Id(card)){
                static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<80)
                    LOG_INFO("CustomComponents/TriggeredGrant/REUSE: card=%d entity=%d count=%d",Engine::Match::Id(card),Engine::Match::Id(entity),Core::Metadata::Bind(counterCount,counter).Get());
                return;
            }
        }
        // No live ability entities remain. Discard stale IDs before recreating.
        if(ids->size){auto clear=BNM::Class(reinterpret_cast<Obj*>(ids)).GetMethod("Clear",0).cast<void>();if(clear.IsValid())Core::Metadata::Bind(clear,reinterpret_cast<Obj*>(ids))();}
    }
    originalCreate(creator,card,counter,method);
}
static Obj* Update(Obj* system,Obj* effect,Obj* card,const void* method){
    const auto options=effects.Get(effect);
    if(!options.enabled)return originalUpdate(system,effect,card,method);
    const int guid=Core::Metadata::Bind(abilityGuid,effect).Get(),source=Core::Metadata::Bind(effectSource,effect).Get();
    Obj* prior=Counter(card,guid);const int count=prior?Core::Metadata::Bind(counterCount,prior).Get():0;
    Policy::Value baseline=prior?Core::Metadata::Bind(counterValue,prior).Get():Policy::Value{};
    auto* state=State(card,true);auto rows=Read(state);const auto historyBefore=rows;
    for(const auto& row:rows)if(row[0]==guid){baseline={row[6],row[7]};break;}
    const auto incoming=Core::Metadata::Bind(effectValue,effect).Get();
    auto existing=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row[0]==guid && row[1]==source && row[2]==options.duration;});
    const bool duplicate=existing!=rows.end();
    Obj* counter=originalUpdate(system,effect,card,method);if(!counter || !state)return counter;
    if(duplicate){
        if(options.operation==2 && (*existing)[5]==2){const auto sum=std::int64_t((*existing)[4])+incoming.amount;(*existing)[4]=static_cast<int>(std::clamp(sum,std::int64_t(INT32_MIN),std::int64_t(INT32_MAX)));}
        else {*existing={guid,source,options.duration,incoming.type,incoming.amount,options.operation,baseline.type,baseline.amount};}
        Core::Metadata::Bind(counterCount,counter).Set(count);
    }else rows.push_back({guid,source,options.duration,incoming.type,incoming.amount,options.operation,baseline.type,baseline.amount});
    Save(state,rows);Core::Metadata::Bind(counterValue,counter).Set(Policy::Evaluate(rows,guid,baseline));
    RecordGrant(card,historyBefore,rows,guid,source,options.duration,options.operation);
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<120)
        LOG_INFO("CustomComponents/TriggeredGrant/APPLY: card=%d guid=%d source=%d duration=%d operation=%d duplicate=%d",Engine::Match::Id(card),guid,source,options.duration,options.operation,int(duplicate));
    return counter;
}
static void Remove(Obj* system,Obj* effect,const void* method){
    static auto modelField=BNM::Class("PvZCards.Core.Systems","RemoveTriggeredAbilitySystem").GetField("<Model>k__BackingField").cast<Obj*>();
    Obj* model=modelField.IsValid()?Core::Metadata::Bind(modelField,system).Get():nullptr;
    Obj* card=Engine::EntityAccess::Find(model,Core::Metadata::Bind(effectTarget,effect).Get());
    auto* state=State(card);auto rows=Read(state);const auto historyBefore=rows;
    static auto guidField=BNM::Class("PvZCards.Engine.Effects","RemoveTriggeredAbilityEffect").GetField("AbilityGuid").cast<int>();
    const int guid=guidField.IsValid()?Core::Metadata::Bind(guidField,effect).Get():-1;
    const int source=Core::Metadata::Bind(effectSource,effect).Get();
    auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value[0]==guid && value[1]==source && value[2]<0;});
    if(row==rows.end())row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value[0]==guid && value[1]==source;});
    if(row!=rows.end()){
        const Policy::Value baseline{(*row)[6],(*row)[7]};rows.erase(row);Save(state,rows);
        Refresh(model,card,guid,baseline,rows);
    }
    originalRemove(system,effect,method);
    RecordGrant(card,historyBefore,rows,guid,source,-1,-1);
}
inline void Expire(Obj* model,int duration){
    const auto api=Engine::EntityAccess::Api();auto* entities=model?Core::Metadata::Bind(api.entities,model).Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return;
    for(int i=0;i<entities->size;++i){
        Obj* card=entities->items->m_Items[i];auto* state=State(card);if(!state)continue;
        auto rows=Read(state);std::vector<Policy::Row> expired;
        for(const auto& row:rows)if(row[2]==duration)expired.push_back(row);
        if(expired.empty())continue;
        static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<80)
            LOG_INFO("CustomComponents/TriggeredGrant/EXPIRE: card=%d duration=%d grants=%zu",Engine::Match::Id(card),duration,expired.size());
        const auto historyBefore=rows;
        for(auto& row:rows)if(row[2]==duration)row[2]=-duration;Save(state,rows);
        RecordGrant(card,historyBefore,rows,-1,-1,duration,-1,true);
        Obj* instances=Engine::Match::Instances(card);std::vector<Obj*> removals;
        std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> roots;
        for(const auto& row:expired){
            Refresh(model,card,row[0],{row[6],row[7]},rows);
            Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","RemoveTriggeredAbilityEffect"));if(!effect)continue;
            roots.push_back(std::make_shared<Core::ManagedRuntime::Root>(effect));
            static auto guidField=BNM::Class("PvZCards.Engine.Effects","RemoveTriggeredAbilityEffect").GetField("AbilityGuid").cast<int>();
            static auto validation=BNM::Class("PvZCards.Engine","Effect").GetField("RequiresTargetValidation").cast<bool>();
            static auto player=BNM::Class("PvZCards.Engine","Effect").GetField("PlayerId").cast<int>();
            static auto origin=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
            if(!guidField.IsValid() || !validation.IsValid() || !player.IsValid() || !origin.IsValid()){Core::ManagedPool::Return(instances,effect);continue;}
            Core::Metadata::Bind(guidField,effect).Set(row[0]);Core::Metadata::Bind(effectSource,effect).Set(row[1]);Core::Metadata::Bind(effectTarget,effect).Set(Engine::Match::Id(card));Core::Metadata::Bind(validation,effect).Set(false);
            Core::Metadata::Bind(player,effect).Set(Engine::BoardPlacement::Owner(card));Core::Metadata::Bind(origin,effect).Set(-1);
            removals.push_back(effect);
        }
        if(!removals.empty() && !Engine::EffectContinuations::Schedule(model,instances,removals))LOG_ERR("CustomComponents/TriggeredGrant: expiry enqueue failed card=%d",Engine::Match::Id(card));
    }
}
inline void Leaving(Obj* model,int id){
    Obj* card=Engine::EntityAccess::Find(model,id);auto* state=State(card);if(!state)return;
    const auto rows=Read(state);
    for(const auto& row:rows)Refresh(model,card,row[0],{row[6],row[7]},{});
    state->Assign(nullptr); // Stock CardResetter removes temporary counters.
    RecordGrant(card,rows,{},-1,-1,-1,-1,true);
}
