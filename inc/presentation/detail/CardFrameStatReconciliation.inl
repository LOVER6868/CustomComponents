#include "diagnostics/DiagnosticLogging.hpp"
// Internal implementation fragment for ZoneTriggeredAbilityPresentationSystem.
// Card-frame bindings, queue-idle refresh and stock presentation observations.
// Included at its original declaration position inside the owning namespace.
// Not a standalone public header; hook installation remains in the parent header.

    inline BNM::Field<int> cardViewEntityId;
    inline BNM::Field<Obj*> cardViewFrame;
    inline BNM::Field<int> frameCurrentAttack;
    inline BNM::Field<int> frameInitialAttack;
    inline BNM::Field<int> frameInitialHealth;
    inline BNM::Field<int> frameInitialSunCost;

    inline BNM::Method<int> componentCalculateAttack;
    inline std::atomic<bool> frameStatsDirty{true};
    inline bool IsMappedCardView(Obj* view){
        static auto type=BNM::Class("PvZCards.Game","CardViewComponent");
        return view && type.IsValid() && BNM::IsA(view,type);
    }
    // Generic hand-entry synchronization. Stock GrantAbilityPresentation remains
    // the sole contributor of custom abilities to SpecialAbilityChecker.
    inline BNM::Field<int> frameMaxHealth;
    inline BNM::Field<int> frameCurrentHealth;
    inline BNM::Field<int> frameCurrentSunCost;
    inline BNM::Field<int> healthCurrentDamage;
    inline BNM::Field<Obj*> healthMaxHealth;
    inline BNM::Field<Obj*> sunCostValue;
    inline BNM::Method<int> calculateModifiableValue;


    inline BNM::Method<void> frameSetSpecialAbilitySprites;
    inline BNM::Class stateClass;
    inline BNM::Class checkerClass;
    inline BNM::Method<void> stateCtor;
    inline BNM::Method<void> checkerCtor;
    inline BNM::Method<void> checkerAddSpecialAbilities;
    inline BNM::Field<Obj*> stateAbilities;
    inline bool traitSyncReady = false;
    struct FrameTraits {bool known=false;std::vector<int> abilities;};
    inline Core::WeakObjectSettings<FrameTraits> frameTraits;
    inline std::shared_ptr<Core::ManagedRuntime::Root> traitChecker;
    // A minimal EntityState is constructed solely for its initialized Abilities
    // list. No factory injection, stat mutation or new hook is needed.

    inline void InvalidateFrameView(Obj*);
    using CardLoadedFn = void (*)(void*, Obj*, const void*);
    inline CardLoadedFn originalCardLoaded = nullptr;

    // VIEWTRACE: diagnostic-only. Never apply a BuffAttack correction during
    // card loading; it is too early to establish the final view association.
    inline void TraceFrameForCard(Obj* view, const char* stage, int mappedId = -1, Obj* model = nullptr) {
        if constexpr (!CustomComponents::Diagnostics::kVerbose) return;
        if (!view) return;
        const int cardId = cardViewEntityId.IsValid() ? cardViewEntityId[view].Get() : -1;
        Obj* frame = cardViewFrame.IsValid() ? cardViewFrame[view].Get() : nullptr;
        const int frameAttack = frame && frameCurrentAttack.IsValid() ? frameCurrentAttack[frame].Get() : -99999;
        int gameplayAttack = -99999;
        Obj* resolvedModel = model;
        const int id = mappedId >= 0 ? mappedId : cardId;
        if(!resolvedModel) {Obj* mapper=Core::SceneViewContext::RootedMapper();if(mapper && viewMapperEntityModel.IsValid()) resolvedModel=viewMapperEntityModel[mapper].Get();}
        if (resolvedModel && id >= 0 && entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[resolvedModel](id)) {
            Obj* entity = getEntityById[resolvedModel](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            if (attack && componentCalculateAttack.IsValid())
                gameplayAttack = componentCalculateAttack[attack]();
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: %s view=%p viewType=%s mappedId=%d cardId=%d model=%p frame=%p frameAttack=%d gameplayAttack=%d",
                 stage, view, TraceObjectType(view), mappedId, cardId, resolvedModel, frame, frameAttack, gameplayAttack);
    }

    static void CardLoadedSyncHook(void* self, Obj* data, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: OnCardLoaded ENTER view=%p data=%p", self, data);
        if (view) TraceFrameForCard(view, "OnCardLoaded BEFORE");
        originalCardLoaded(self, data, method);
        // The mapper may have bound a prefab before its async frame existed.
        // Mark the completed asset boundary for both local and opponent views.
        frameStatsDirty = true;
        InvalidateFrameView(view);
        if (view) TraceFrameForCard(view, "OnCardLoaded AFTER");
    }

    using AddViewObjectFn = void (*)(void*, int, Obj*, const void*);
    using ShowInHandFn = void (*)(void*, const void*);
    inline AddViewObjectFn originalAddViewObject = nullptr;
    inline ShowInHandFn originalShowInHand = nullptr;

    // Exact model/entity/view association, captured only after stock mapper registration.
    // This avoids guessing the EntityModel from a CardId shared across matches.
    struct FrameViewBinding { Obj* view = nullptr; Obj* model = nullptr; int id = -1; bool shownInHand = false;
        // Never trust an entity ID across an association change or a new view.
        std::uint64_t lastRevision = ~std::uint64_t{0}; bool explicitDirty = true;
    };
    inline std::array<FrameViewBinding, 256> frameViewBindings{};
    inline void InvalidateFrameView(Obj* view){
        if(!view)return;
        for(auto& binding:frameViewBindings)
            if(binding.view==view){binding.explicitDirty=true;frameStatsDirty=true;}
    }
    // A new EntityModel means a new match's CardViews. Never reuse pointers
    // or entity IDs bound to the previous scene.
    inline Obj* boundFrameModel = nullptr;
    inline void InvalidateCard(int id){
        if(id<0){frameStatsDirty=true;return;}
        const auto model=Core::SceneViewContext::ModelIdentity();
        for(auto& binding:frameViewBindings){
            if(binding.id!=id || !binding.view ||
               (model && reinterpret_cast<std::uintptr_t>(binding.model)!=model))continue;
            binding.explicitDirty=true;
            frameStatsDirty=true;
        }
    }


    // Deferred *visual-only* verification for a stock-created lane frame.
    // Return tickets exist only on the client that ran the custom transfer;
    // the opponent also needs a frame-to-model comparison after animation.
    // Keep numeric identities here: the scene-owned mapper is resolved through
    // CardViewLifecycleCoordinator's existing GC handle before any object access.
    struct PendingIdleLaneFrame {
        std::uintptr_t model = 0;
        std::uintptr_t mapper = 0;
        std::uintptr_t view = 0;
        std::uintptr_t animation = 0;
        std::uintptr_t queue = 0;
        int id = -1;
        bool completed = false;
        unsigned queueProbes = 0;
    };
    inline std::array<PendingIdleLaneFrame, 128> pendingIdleLaneFrames{};
    inline thread_local std::uintptr_t activeQueueUpdate = 0;

    inline void RememberIdleLaneFrame(Obj* animation, Obj* mapper, Obj* model,
                                      Obj* view, int id) {
        if (!animation || !mapper || !model || !view || id < 0) return;
        // A second ShowCard for the same animation must not schedule two buffs.
        for (auto& entry : pendingIdleLaneFrames) {
            if (entry.animation == reinterpret_cast<std::uintptr_t>(animation) ||
                (entry.model == reinterpret_cast<std::uintptr_t>(model) &&
                 entry.id == id)) {
                entry = {reinterpret_cast<std::uintptr_t>(model),
                         reinterpret_cast<std::uintptr_t>(mapper),
                         reinterpret_cast<std::uintptr_t>(view),
                         reinterpret_cast<std::uintptr_t>(animation),
                         activeQueueUpdate, id, false, 0};
                return;
            }
        }
        for (auto& entry : pendingIdleLaneFrames) {
            if (!entry.model) {
                entry = {reinterpret_cast<std::uintptr_t>(model),
                         reinterpret_cast<std::uintptr_t>(mapper),
                         reinterpret_cast<std::uintptr_t>(view),
                         reinterpret_cast<std::uintptr_t>(animation),
                         activeQueueUpdate, id, false, 0};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/IDLE-STAT: pending table full id=%d", id);
    }

    inline void CompleteIdleLaneFrame(Obj* animation) {
        if (!animation) return;
        for (auto& entry : pendingIdleLaneFrames)
            if (entry.animation == reinterpret_cast<std::uintptr_t>(animation))
                entry.completed = true;
    }
    inline void RefreshFrameTraits(Obj* frame,Obj* entity) {
        if(!frame || !entity || !traitSyncReady || !frameSetSpecialAbilitySprites.IsValid()) return;
        static auto type=BNM::Class("PvZCards.Game","CardFrameViewComponent");
        static auto icons=type.GetField("CardFrameIconView").cast<Obj*>();
        static auto board=type.GetField("CardFrameOnBoardSpriteView").cast<Obj*>();
        if(!icons.IsValid() || !board.IsValid() || !Core::Metadata::Bind(icons,frame).Get() || !Core::Metadata::Bind(board,frame).Get())return;
        // Scratch is only used to compare signatures. Stock presentation
        // receives its own state and can retain it across asynchronous work.
        static thread_local bool probing=false;
        if(probing)return;
        struct ProbeScope {
            bool& active;
            explicit ProbeScope(bool& flag):active(flag){active=true;}
            ~ProbeScope(){active=false;}
        } scope(probing);
        static thread_local std::unique_ptr<Core::ManagedRuntime::Root> scratch;
        if(!scratch || !scratch->Get()){
            scratch=std::make_unique<Core::ManagedRuntime::Root>(stateClass.CreateNewInstance());
            if(!scratch->Get())return;
            stateCtor[scratch->Get()]();
        }
        if(!traitChecker || !traitChecker->Get()){
            traitChecker=std::make_shared<Core::ManagedRuntime::Root>(checkerClass.CreateNewInstance());
            if(traitChecker->Get())Core::Metadata::Bind(checkerCtor,traitChecker->Get())();
        }
        Obj* checker=traitChecker?traitChecker->Get():nullptr;
        if(!checker)return;
        Obj* abilities=stateAbilities[scratch->Get()].Get();
        if(!abilities)return;
        static const auto clearMetadata=BNM::Class(abilities).GetMethod("Clear",0).cast<void>();
        auto clear=clearMetadata;
        if(!clear.IsValid())return;
        clear[abilities]();
        checkerAddSpecialAbilities[checker](abilities,entity);
        const auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(abilities);
        if(!Engine::EntityAccess::Valid(list))return;
        const auto first=list->items->m_Items;
        const auto last=first+list->size;
        const auto previous=frameTraits.Get(frame);
        if(previous.known && std::equal(first,last,previous.abilities.begin(),previous.abilities.end()))return;
        std::vector<int> signature(first,last);
        Obj* state=stateClass.CreateNewInstance();
        Core::ManagedRuntime::Root stateRoot(state);
        if(!stateRoot.Get())return;
        stateCtor[state]();
        Obj* presentationAbilities=stateAbilities[state].Get();
        if(!presentationAbilities)return;
        checkerAddSpecialAbilities[checker](presentationAbilities,entity);
        // Rebuilding unchanged sprites can restart native icon presentation
        // and load assets during unrelated plays/Level changes.
        const auto error=BNM::TryInvoke([&]{Core::Metadata::Bind(frameSetSpecialAbilitySprites,frame)(state);});
        if(!error.IsValid())frameTraits.Set(frame,{true,std::move(signature)});
    }
    inline void BindFrameView(Obj* view, Obj* model, int id) {
        if (!view || !model || id < 0) return;
        frameStatsDirty = true;
        if (boundFrameModel != model) {
            frameViewBindings.fill(FrameViewBinding{});
            pendingIdleLaneFrames.fill(PendingIdleLaneFrame{});

            boundFrameModel = model;
        }
        // A returned entity gets a NEW CardView. Replace its previous
        // binding instead of keeping a dead pointer until the table fills.
        for (auto& binding : frameViewBindings) {
            if (binding.view == view ||
                (binding.model == model && binding.id == id)) {
                binding = {view, model, id};
                return;
            }
        }
        for (auto& binding : frameViewBindings) {
            if (!binding.view) {
                binding = {view, model, id};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: view binding table full id=%d", id);
    }

    static void AddViewObjectTraceHook(void* self, int entityId, Obj* view, const void* method) {
        Obj* mapper = static_cast<Obj*>(self);
        Obj* model = mapper && viewMapperEntityModel.IsValid() ? viewMapperEntityModel[mapper].Get() : nullptr;
        // I.10B: the frontend mapper is already active when stock views are
        // registered. Capture it here, before any discard-to-lane preflight;
        // waiting for CreateCardAnimation.OnStarted was too late. This uses
        // the existing read-only hook and a scene-scoped GC root.
        Core::SceneViewContext::Capture(mapper, model);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject ENTER mapper=%p model=%p entity=%d view=%p type=%s",
                 self, model, entityId, view, TraceObjectType(view));
        // Do not assume every ViewObject is CardViewComponent.
        if (view && IsMappedCardView(view))
            TraceFrameForCard(view, "AddViewObject BEFORE", entityId, model);
        originalAddViewObject(self, entityId, view, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject EXIT mapper=%p entity=%d view=%p",
                 self, entityId, view);
        if (view && IsMappedCardView(view)) {
            BindFrameView(view, model, entityId);
            TraceFrameForCard(view, "AddViewObject AFTER", entityId, model);
        }
        Engine::Events::afterViewAdded.Notify({mapper,model,entityId,view});
    }

    static void ShowInHandTraceHook(void* self, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        if (view) TraceFrameForCard(view, "ShowInHand BEFORE");
        originalShowInHand(self, method);
        // Stock hand presentation completes first. Never change gameplay Attack.
        frameStatsDirty = true;
        for(auto& binding:frameViewBindings)
            if(view && binding.view==view && cardViewEntityId.IsValid() &&
               binding.id==cardViewEntityId[view].Get()) {
                binding.shownInHand=true;binding.explicitDirty=true;
            }
        if (view) TraceFrameForCard(view, "ShowInHand AFTER");
        Engine::Events::afterShowInHand.Notify(view);
    }
