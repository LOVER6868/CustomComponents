#pragma once
#include "presentation/detail/Pick3DiagnosticMode.hpp"
// Native CardViewFactory avatar loader: three independent offers, no gameplay entity.
// Each avatar lives far outside the battlefield and renders on layer 31 through
// its own optional isolation camera into a UGUI RenderTexture.
// Safety: session tickets, managed callback GC roots, request cancellation,
// stock CardAssetUnloader, quarantined callbacks after cancellation.
#include <cstdint>
#include <cstdio>
#include <BNM/Method.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Delegates.hpp>
#include <BNM/Exceptions.hpp>
#include "presentation/detail/Pick3NativeAvatarReceiver.hpp"
#include "presentation/detail/Pick3NativeAvatarRender.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3NativeAvatarLoad {
    using Obj = BNM::IL2CPP::Il2CppObject;
    namespace Callbacks = Pick3NativeAvatarReceiver;
    struct Vector3 {float x,y,z;};

    enum class MotionMode : unsigned char { Unknown, Fighter, Environment, Other };
    inline const char* MotionName(MotionMode mode) {
        switch (mode) {
            case MotionMode::Fighter: return "fighter";
            case MotionMode::Environment: return "environment";
            case MotionMode::Other: return "nonfighter";
            default: return "unknown";
        }
    }
    inline MotionMode Classify(const Pick3CardMetadata::CardInfo& info) {
        if (!info.hasCardData) return MotionMode::Unknown;
        if (info.fighter) return MotionMode::Fighter;
        return info.environment ? MotionMode::Environment : MotionMode::Other;
    }
    inline BNM::Class factoryClass, cardViewClass, cardViewComponentClass, avatarClass, configClass, receiverClass,
                      actionClass, gameObjectClass, transformClass, assetUnloaderClass, animatorParamsClass, spriteRendererClass;
    inline BNM::Method<Obj*> createCard, createCardWithFactoryRoot, getAvatar, getAvatarObject, getTransform, getInHandSpriteRenderer, getSpriteRendererObject, getHandSprite;
    inline BNM::Method<void> showForInspection;
    inline BNM::Method<Vector3> getDesiredScale, getDesiredAvatarScale;
    inline BNM::Method<void> handleEnter, playIdle, setInHandTrigger, setPosition, unloadAssets, destroyObject;
    inline BNM::Field<Obj*> inHandTrigger;
    inline BNM::Field<bool> configAvatar, configFrame, configCardBack,
        configInHandSprite, configTargetIcon, configGravestone, configInGameEffects;
    // Controls are read per request; ADB changes need no APK rebuild after installation.
    // File: pick3_native_test.txt: overload(0=explicit,1=factory root) avatar inHand inspection phase(0=call,1=config,2=delegate,3=receiver-callback)
    struct NativeTestControls { int overload=0, avatar=1, inHand=1, inspection=1, phase=0; };
    inline NativeTestControls ReadNativeTestControls() {
        NativeTestControls c{};
        constexpr const char* path="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_native_test.txt";
        if (FILE* f=std::fopen(path,"r")) {
            int a=0,b=1,d=1,e=1,phase=0;
            const int n=std::fscanf(f,"%d %d %d %d %d",&a,&b,&d,&e,&phase);
            if(n>=4) {
                c.overload=a==1?1:0;
                c.avatar=b!=0;
                c.inHand=d!=0;
                c.inspection=e!=0;
                c.phase=n==5 && phase>=1 && phase<=3 ? phase:0;
            }
            std::fclose(f);
        }
        return c;
    }
    inline bool enabled = false, initialized = false;
    inline std::uint32_t serial = 1;

    inline std::uint32_t Root(Obj* p) {
        return p && Core::SceneViewContext::newGCHandle ?
            Core::SceneViewContext::newGCHandle(p,false):0;
    }
    inline Obj* Target(std::uint32_t root) {
        return root && Core::SceneViewContext::getGCHandle ?
            Core::SceneViewContext::getGCHandle(root):nullptr;
    }
    inline void Free(std::uint32_t& root) {
        if (root && Core::SceneViewContext::freeGCHandle)
            Core::SceneViewContext::freeGCHandle(root);
        root=0;
    }

    struct Request {
        bool used=false, retired=false, entered=false, attempted=false, enterPlayed=false,
             idlePlayed=false, processed=false, exceptionThrown=false;
        int guid=-1,option=-1;
        std::uint32_t ticket=0, receiverRoot=0, delegateRoot=0, configRoot=0,
                      parentRoot=0, loadRoot=0, unloaderRoot=0;
        float enterTime=0.f;
        NativeTestControls test{};
        int envFramePass=0;
        int renderProbeCount=0;
        MotionMode motion=MotionMode::Unknown;
    };
    inline Request slots[Callbacks::kSlots]{};
    inline int currentSlots[3]{-1,-1,-1};
    // Only used by opt-in sequential stage 4. A callback can be complete
    // before the CardView finishes initializing, so require processing too.
    inline bool OptionFinished(int option){
        if(option<0 || option>=3) return true;
        const int i=currentSlots[option];
        if(i<0 || !slots[i].used) return true;
        const Request& s=slots[i];
        return s.retired || (Callbacks::Done(i,s.ticket) && s.processed &&
                            (s.idlePlayed || s.renderProbeCount>=90));
    }
    inline bool popupOpen=false, warnedPool=false, warnedDependency=false;
    inline bool warnedGate=false, lateInitTried=false;

    inline bool Initialize(bool requested) {
        enabled=requested;
        if (!requested) return true;
        factoryClass=BNM::Class("PvZCards.Game","CardViewFactory");
        cardViewClass=BNM::Class("PvZCards.Game","CardView"); // interface for Action<T>
        cardViewComponentClass=BNM::Class("PvZCards.Game","CardViewComponent");
        spriteRendererClass=BNM::Class("UnityEngine","SpriteRenderer");
        if (spriteRendererClass.IsValid()) {
            getSpriteRendererObject=BNM::Class("UnityEngine","Component").GetMethod("get_gameObject",0).cast<Obj*>();
            getHandSprite=spriteRendererClass.GetMethod("get_sprite",0).cast<Obj*>();
        }
        avatarClass=BNM::Class("PvZCards.Game","CardAvatarViewComponent");
        configClass=BNM::Class("PvZCards.Game","CardViewConfiguration");
        receiverClass=BNM::Class("Sprig.Pick3","NativeAvatarLoadReceiver");
        gameObjectClass=BNM::Class("UnityEngine","GameObject");
        transformClass=BNM::Class("UnityEngine","Transform");
        assetUnloaderClass=BNM::Class("PvZCards.Game","CardAssetUnloader");
        // AnimatorParameters is in the global namespace in this IL2CPP build.
        // This is the stock in-hand/card-view state, NOT the environment
        // lane/board idle state entered by HandlePlayedToBoard().
        animatorParamsClass=BNM::Class("", "AnimatorParameters");
        if (animatorParamsClass.IsValid())
            inHandTrigger=animatorParamsClass.GetField("InHandTrigger").cast<Obj*>();
        if(factoryClass.IsValid()) {
            createCard=factoryClass.GetMethod("CreateCardAsync",4).cast<Obj*>();
            createCardWithFactoryRoot=factoryClass.GetMethod("CreateCardAsync",3).cast<Obj*>();
        }
        if(cardViewComponentClass.IsValid()) {
            getAvatar=cardViewComponentClass.GetMethod("GetAvatar",0).cast<Obj*>();
            getInHandSpriteRenderer=cardViewComponentClass.GetMethod("GetInHandSpriteRenderer",0).cast<Obj*>();
            showForInspection=cardViewComponentClass.GetMethod("ShowForInspection",0).cast<void>();
            getDesiredScale=cardViewComponentClass.GetMethod("GetDesiredScale",0).cast<Vector3>();
            getDesiredAvatarScale=cardViewComponentClass.GetMethod("GetDesiredAvatarScale",0).cast<Vector3>();
        }
        if(avatarClass.IsValid()) {
            handleEnter=avatarClass.GetMethod("HandlePlayedToBoard",0).cast<void>();
            playIdle=avatarClass.GetMethod("PlayIdle",0).cast<void>();
            getAvatarObject=avatarClass.GetMethod("GetGameObject",0).cast<Obj*>();
            setInHandTrigger=avatarClass.GetMethod("SetTrigger",1).cast<void>();
        }
        if(configClass.IsValid()) {
            configAvatar=configClass.GetField("Avatar").cast<bool>();
            configFrame=configClass.GetField("Frame").cast<bool>();
            configCardBack=configClass.GetField("CardBack").cast<bool>();
            configInHandSprite=configClass.GetField("InHandSprite").cast<bool>();
            configTargetIcon=configClass.GetField("TargetIcon").cast<bool>();
            configGravestone=configClass.GetField("Gravestone").cast<bool>();
            configInGameEffects=configClass.GetField("InGameEffects").cast<bool>();
        }
        if(gameObjectClass.IsValid()) getTransform=gameObjectClass.GetMethod("get_transform",0).cast<Obj*>();
        if(transformClass.IsValid()) setPosition=transformClass.GetMethod("set_position",1).cast<void>();
        if(assetUnloaderClass.IsValid()) unloadAssets=assetUnloaderClass.GetMethod("UnloadAssets",1).cast<void>();
        destroyObject=BNM::Class("UnityEngine","Object").GetMethod("Destroy",1).cast<void>();
        if(cardViewClass.IsValid())
            actionClass=BNM::Class("System","Action`1").GetGeneric({cardViewClass.GetCompileTimeClass()});
        initialized=factoryClass.IsValid() && cardViewClass.IsValid() &&
            cardViewComponentClass.IsValid() && avatarClass.IsValid() &&
            configClass.IsValid() && receiverClass.IsValid() &&
            receiverClass.GetMethod("OnLoaded",1).IsValid() && actionClass.IsValid() &&
            createCard.IsValid() && getAvatar.IsValid() && getAvatarObject.IsValid() &&
            showForInspection.IsValid() && configAvatar.IsValid() &&
            configFrame.IsValid() && configCardBack.IsValid() && configInHandSprite.IsValid() &&
            configTargetIcon.IsValid() && configGravestone.IsValid() && configInGameEffects.IsValid() &&
            gameObjectClass.IsValid() && getTransform.IsValid() && setPosition.IsValid() &&
            assetUnloaderClass.IsValid() && unloadAssets.IsValid() && destroyObject.IsValid() &&
            Core::SceneViewContext::newGCHandle && Core::SceneViewContext::getGCHandle &&
            Core::SceneViewContext::freeGCHandle;
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: INIT ready=%d receiver=%d callback=%d action=%d create=%d avatar=%d unloader=%d unloadMethod=%d roots=%d options=3 renderToPortrait=1",
                 initialized?1:0, receiverClass.IsValid()?1:0,
                 receiverClass.IsValid() && receiverClass.GetMethod("OnLoaded",1).IsValid()?1:0,
                 actionClass.IsValid()?1:0, createCard.IsValid()?1:0,
                 handleEnter.IsValid() && playIdle.IsValid()?1:0,
                 assetUnloaderClass.IsValid()?1:0,unloadAssets.IsValid()?1:0,
                 Core::SceneViewContext::newGCHandle && Core::SceneViewContext::getGCHandle &&
                 Core::SceneViewContext::freeGCHandle ? 1:0);
        return true; // optional experiment, must never break normal PickFromThree.
    }

    // Staged, explicit opt-in for risky native-card creation. Defaults to safe
    // diagnostic mode 0 even when pick3_factory_probe.txt is 5/6/7.
    // 0=skip; 1=binder; 2=injection; 3=factory; 4=unloader; 5=full creation.
    // Keep the release opt-in independent of stale diagnostic stage files.
    inline bool releaseCreationAllowed=false;
    inline int FactoryDiStage() {
        constexpr const char* path =
            "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_factory_di_stage.txt";
        FILE* f = std::fopen(path, "r");
        if (!f) return 0;
        int value=0;
        const int read=std::fscanf(f,"%d",&value);
        std::fclose(f);
        return read==1 && value>=0 && value<=5 ? value : 0;
    }

    inline Obj* ResolveInjected(BNM::Class klass, const char* label) {
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=class valid=%d",
                 label,klass.IsValid());
        if (!klass.IsValid()) return nullptr;
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=before_binder root=%d",
                 label,Pick3::Submission::binderRoot!=0);
        Obj* binder=Pick3::Submission::Target(Pick3::Submission::binderRoot);
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=after_binder found=%d",
                 label,binder!=nullptr);
        if (!binder || !Pick3::Submission::getInjectionBinder.IsValid() ||
            !Pick3::Submission::getInjectedInstance.IsValid()) return nullptr;
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=before_get_injection",label);
        Obj* injection=nullptr;
        auto err=BNM::TryInvoke([&]{injection=Pick3::Submission::getInjectionBinder[binder]();});
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=after_get_injection found=%d exception=%d",
                 label,injection!=nullptr,err.IsValid());
        if(err.IsValid() || !injection) return nullptr;
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=before_get_service",label);
        Obj* service=nullptr;
        err=BNM::TryInvoke([&]{service=Pick3::Submission::getInjectedInstance[injection](
            klass.GetMonoType(),static_cast<Obj*>(nullptr));});
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=after_get_service found=%d exception=%d",
                 label,service!=nullptr,err.IsValid());
        if (err.IsValid() || !service) return nullptr;
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=before_type_check",label);
        const bool valid=BNM::IsA(service,klass);
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: %s step=after_type_check valid=%d",label,valid);
        return valid?service:nullptr;
    }

    inline void Retire(int index) {
        Request& s=slots[index];
        if(!s.used || s.retired) return;
        s.retired=true;
        Obj* req=Target(s.loadRoot);
        if(s.entered && req && !Callbacks::Done(index,s.ticket)) {
            auto cancel=BNM::Class(req).GetMethod("Cancel",0).cast<void>();
            if(cancel.IsValid()) {
                const auto ex=BNM::TryInvoke([&]{cancel[req]();});
                LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: CANCEL guid=%d ok=%d",s.guid,!ex.IsValid());
            } else LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: CANCEL_UNAVAILABLE guid=%d",s.guid);
        }
    }
    inline void EndSession() {
        popupOpen=false;
        Pick3NativeAvatarRender::EndSession();
        for(int option=0;option<3;++option){
            if(currentSlots[option]>=0){Retire(currentSlots[option]);currentSlots[option]=-1;}
        }
    }
    inline void Cleanup(int index) {
        Request& s=slots[index];
        if(!s.used) return;
        auto cardHandle=Callbacks::cardHandles[index].exchange(0,std::memory_order_acq_rel);
        Obj* card=Target(cardHandle);
        Obj* unloader=Target(s.unloaderRoot);
        if(card && unloader && unloadAssets.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{unloadAssets[unloader](card);});
            LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: UNLOAD guid=%d ok=%d",s.guid,!ex.IsValid());
        }
        if(cardHandle && Core::SceneViewContext::freeGCHandle)
            Core::SceneViewContext::freeGCHandle(cardHandle);
        Pick3NativeAvatarRender::Stop(s.option,s.ticket);
        Obj* parent=Target(s.parentRoot);
        if(parent && destroyObject.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{destroyObject(parent);});
            if(ex.IsValid()) LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: PARENT_DESTROY_ERROR guid=%d",s.guid);
        }
        Free(s.configRoot);Free(s.parentRoot);Free(s.loadRoot);
        Free(s.unloaderRoot);Free(s.receiverRoot);Free(s.delegateRoot);
        Callbacks::Disarm(index);
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: RELEASE guid=%d slot=%d",s.guid,index);
        s=Request{};
    }
    inline void BeginSession() {
        popupOpen=true;for(int& index:currentSlots)index=-1;warnedPool=false;warnedDependency=false;
        warnedGate=false;lateInitTried=false;
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: SESSION_BEGIN enabled=%d initialized=%d",
                 enabled?1:0,initialized?1:0);
    }

    inline bool Start(int guid,int option,const Pick3CardMetadata::CardInfo& metadata) {
        // Module init can precede late custom-class resolution on some
        // devices. Retry metadata resolution exactly once per popup session.
        if (enabled && popupOpen && !initialized && !lateInitTried) {
            lateInitTried=true;
            Initialize(true);
            LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: LATE_INIT ready=%d",initialized?1:0);
        }
        if(!enabled || !initialized || !popupOpen || option<0 || option>=3 ||
            currentSlots[std::clamp(option,0,2)]>=0 || guid<=0) {
            if(!warnedGate) {
                warnedGate=true;
                LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: GATE_REJECT option=%d guid=%d enabled=%d initialized=%d session=%d",
                        option+1,guid, enabled?1:0, initialized?1:0, popupOpen?1:0);
            }
            return false;
        }
        // Diagnostic checkpoints for the recurring 0x38 IL2CPP null dereference.
        // The faulty build logged FACTORY_READY but never logged AVATAR_RENDER: STAGE.
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: CACHED_CARD option=%d guid=%d available=%d fighter=%d environment=%d nativeMetadataCalls=0",option+1,guid,metadata.hasCardData?1:0,metadata.fighter?1:0,metadata.environment?1:0);
        if (!metadata.hasCardData) return false;  // Preserve static image if picker metadata is unavailable.
        const MotionMode mode=Classify(metadata);
        // Tricks and powers use the game's actual IN-HAND sprite task.
        // Their one-shot avatar 'idle' has no loop and can disappear entirely.
        // Environments remain native CardAvatar with InHandTrigger.
        const bool useHandSprite=mode==MotionMode::Other;
        // Allow inspection even when this binary has no in-hand fallback API;
        // the loaded avatar still works for all other card types.
        const int diStage=releaseCreationAllowed ? 5 : FactoryDiStage();
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: GATE stage=%d option=%d guid=%d (5=card-create)",diStage,option+1,guid);
        if(diStage==0) return false;
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: STAGE1 binderRoot=%d",Pick3::Submission::binderRoot!=0);
        if(diStage==1) return false;
        Obj* binder=Pick3::Submission::Target(Pick3::Submission::binderRoot);
        LOG_INFO("CustomComponents/Pick3/FACTORY_DI: STAGE2 binder=%d injectionMethod=%d",binder!=nullptr,Pick3::Submission::getInjectionBinder.IsValid());
        if(diStage==2 || !binder) return false;
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: START_STEP option=%d guid=%d step=factory_di_begin",option+1,guid);
        Obj* factory=ResolveInjected(factoryClass,"factory");
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: START_STEP option=%d guid=%d step=factory_di_done found=%d",option+1,guid,factory!=nullptr);
        if(diStage==3) return false;
        Obj* unloader=ResolveInjected(assetUnloaderClass,"unloader");
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: START_STEP option=%d guid=%d step=unloader_di_done found=%d",option+1,guid,unloader!=nullptr);
        if(diStage==4) return false;
        if(!factory || !unloader) {
            if(!warnedDependency) {
                LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: DI_MISSING factory=%d assetUnloader=%d; waiting without blocking popup",
                        factory!=nullptr,unloader!=nullptr);
                warnedDependency=true;
            }
            return false;
        }
        int index=-1;
        for(int i=0;i<Callbacks::kSlots;++i) if(!slots[i].used){index=i;break;}
        if(index<0) {
            if(!warnedPool) LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: POOL_FULL cap=%d; static art retained",Callbacks::kSlots);
            warnedPool=true;return false;
        }
        auto& s=slots[index];s=Request{};
        s.used=true;s.guid=guid;s.option=option;s.motion=mode;s.ticket=++serial;
        if(!serial) s.ticket=++serial;
        s.test=ReadNativeTestControls();
        LOG_INFO("CustomComponents/Pick3/ADB_TEST: option=%d guid=%d overload=%d avatar=%d inHand=%d inspection=%d phase=%d threeArgAvailable=%d",
                 option+1,guid,s.test.overload,s.test.avatar,s.test.inHand,s.test.inspection,s.test.phase,
                 createCardWithFactoryRoot.IsValid()?1:0);
        Callbacks::Arm(index,s.ticket);
        auto fail=[&](){Cleanup(index);return false;};
        s.unloaderRoot=Root(unloader);
        if(!s.unloaderRoot) return fail();
        // An off-screen, scene-owned root prevents native animated objects from
        // drawing on the live battlefield before a proper RT renderer exists.
        Obj* parent=gameObjectClass.CreateNewObjectParameters(BNM::CreateMonoString("Sprig_Pick3_NativeAvatar_Offscreen"));
        s.parentRoot=Root(parent);
        if(!s.parentRoot) return fail();
        // The old offscreen Transform.set_position call consistently faults
        // within IL2CPP (0x38) on the target game build. Do not invoke it.
        // Keep the scene GameObject rooted until completion/cleanup instead.
        // The RT renderer owns the inspection camera and output isolation.
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: START_STEP option=%d guid=%d step=position_skipped_safe",option+1,guid);
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: START_STEP option=%d guid=%d step=render_stage_begin",option+1,guid);
        Pick3NativeAvatarRender::BeginStage(option,parent,s.ticket,
            metadata.environment?1:(metadata.fighter?0:2));
        Obj* config=configClass.CreateNewInstance();
        s.configRoot=Root(config);
        if(!s.configRoot) return fail();
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: CONFIG_ROOT option=%d config=%p root=%u",option+1,config,s.configRoot);
        // Ask the game to load both assets when available: an animated avatar
        // for stock inspection states, plus the stable card-face fallback.
        // The actual CardView, not a guessed Animator trigger, decides which
        // visuals should be active in the inspection state.
        configAvatar[config].Set(s.test.avatar!=0);
        configFrame[config].Set(false);
        configCardBack[config].Set(false);
        configInHandSprite[config].Set(s.test.inHand!=0);
        configTargetIcon[config].Set(false);
        configGravestone[config].Set(false);
        configInGameEffects[config].Set(false);
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: CONFIG_DONE option=%d",option+1);
        if(s.test.phase==1) {
            LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: STOP_PHASE_1 config_complete no_factory_call");
            Cleanup(index); return true;
        }
        auto* receiver=reinterpret_cast<Callbacks::Receiver*>(receiverClass.CreateNewInstance());
        s.receiverRoot=Root(reinterpret_cast<Obj*>(receiver));
        if(!s.receiverRoot) return fail();
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: RECEIVER_ROOT option=%d receiver=%p root=%u",option+1,receiver,s.receiverRoot);
        receiver->slot=index;receiver->ticket=s.ticket;
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: RECEIVER_FIELDS slot=%d ticket=%u",receiver->slot,receiver->ticket);
        auto method=receiverClass.GetMethod("OnLoaded",1);
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: CALLBACK_METHOD valid=%d",method.IsValid()?1:0);
        if(!method.IsValid()) return fail();
        method.SetInstance(reinterpret_cast<Obj*>(receiver));
        auto* shell=reinterpret_cast<BNM::DelegateBase*>(actionClass.CreateNewInstance());
        Obj* delegate=shell ? reinterpret_cast<Obj*>(shell->Create(method)):nullptr;
        s.delegateRoot=Root(delegate);
        if(!s.delegateRoot) return fail();
        LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: DELEGATE_READY option=%d delegate=%p root=%u",option+1,delegate,s.delegateRoot);
        if(s.test.phase==2) {
            LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: STOP_PHASE_2 delegate_complete no_factory_call");
            Cleanup(index); return true;
        }
        if(s.test.phase==3) {
            LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: CALLBACK_SELFTEST_BEGIN");
            receiver->OnLoaded(nullptr);
            LOG_INFO("CustomComponents/Pick3/INVOKE_PROBE: CALLBACK_SELFTEST_END completed=%d",Callbacks::Done(index,s.ticket)?1:0);
            Cleanup(index); return true;
        }
        currentSlots[option]=index;
        s.entered=true; // If managed invocation throws, callback may still run.
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: REQUEST option=%d guid=%d slot=%d kind=%s renderSource=%s isolated=1",
                 option+1,guid,index,MotionName(mode),"inspection+inhand-fallback");
        Obj* request=nullptr;
        Pick3DiagnosticMode::Mark("factory-createCard",option+1,guid);
        LOG_INFO("CustomComponents/Pick3/ADB_TEST: FACTORY_CALL_BEGIN option=%d overload=%d",option+1,s.test.overload);
        const auto ex=BNM::TryInvoke([&]{
            if(s.test.overload==1 && createCardWithFactoryRoot.IsValid())
                request=createCardWithFactoryRoot[factory](guid,config,delegate);
            else
                request=createCard[factory](guid,config,parent,delegate);
        });
        LOG_INFO("CustomComponents/Pick3/ADB_TEST: FACTORY_CALL_END option=%d exception=%d request=%p",option+1,ex.IsValid()?1:0,request);
        Pick3DiagnosticMode::Complete("factory-createCard",option+1,guid);
        if(ex.IsValid()) {
            s.exceptionThrown=true;
            LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: CREATE_EXCEPTION guid=%d class=%s",guid,ex.ClassName().c_str());
            EndSession();return false;
        }
        s.loadRoot=Root(request);
        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: REQUEST_RETURN guid=%d request=%p callback=%d",guid,request,
                 Callbacks::Done(index,s.ticket)?1:0);
        return true;
    }

    inline void Tick(float now) {
        if(!enabled || !initialized) return;
        for(int i=0;i<Callbacks::kSlots;++i) {
            Request& s=slots[i];if(!s.used)continue;
            const bool done=Callbacks::Done(i,s.ticket);
            if(s.retired && (!s.entered || done)) {Cleanup(i);continue;}
            if(!done || s.retired)continue;
            Obj* card=Target(Callbacks::cardHandles[i].load(std::memory_order_acquire));
            if(!s.processed) {
                s.processed=true;
                if(!card || !BNM::IsA(card,cardViewComponentClass)) {
                    LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: CALLBACK_NULL_OR_TYPE guid=%d",s.guid);
                    Retire(i);continue;
                }
                // Let the stock card view choose inspection visuals, animator
                // state and per-prefab transforms. This also triggers the
                // environment's card-view (not board) idle variant.
                bool inspectionOk=false;
                LOG_INFO("CustomComponents/Pick3/ADB_TEST: CALLBACK_PROCESSED option=%d card=%p inspection=%d",s.option+1,card,s.test.inspection);
                if(s.test.inspection && showForInspection.IsValid()) {
                    LOG_INFO("CustomComponents/Pick3/ADB_TEST: INSPECTION_BEGIN option=%d",s.option+1);
                    const auto ex=BNM::TryInvoke([&]{showForInspection[card]();});
                    inspectionOk=!ex.IsValid();
                    LOG_INFO("CustomComponents/Pick3/ADB_TEST: INSPECTION_END option=%d exception=%d",s.option+1,ex.IsValid()?1:0);
                }
                LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: INSPECTION option=%d guid=%d kind=%s success=%d",
                         s.option+1,s.guid,MotionName(s.motion),inspectionOk?1:0);
                Vector3 desired{1,1,1}, avatarShrink{1,1,1};
                if(getDesiredScale.IsValid()) BNM::TryInvoke([&]{desired=getDesiredScale[card]();});
                if(getDesiredAvatarScale.IsValid()) BNM::TryInvoke([&]{avatarShrink=getDesiredAvatarScale[card]();});
                LOG_INFO("CustomComponents/Pick3/CARD_SCALE: option=%d guid=%d desired=(%.3f,%.3f,%.3f) avatar=(%.3f,%.3f,%.3f)",
                         s.option+1,s.guid,desired.x,desired.y,desired.z,
                         avatarShrink.x,avatarShrink.y,avatarShrink.z);
                // Inspection is the ONLY game-owned presentation transition.
                // Do not force HandlePlayedToBoard: some prefabs already enter
                // during inspection, and firing both replays their animation.
                // Renderer readiness is not guaranteed when the managed
                // CardViewFactory callback returns. The avatar hierarchy can
                // finish initialization several UI ticks later; don't show a
                // card-frame/hand-sprite placeholder immediately.
                // No manual fighter Enter, forced Idle, or timed environment reframing.
                // The game's AnimatorController remains the sole animation owner.
                // Inspection Animator state and native prefab remain alive
                // until the popup is dismissed by the existing cleanup path.
            }
            // Exactly one inspection transition. Bounded renderer polling
            // does not replay HandlePlayedToBoard, Enter, or Idle animations.
            if(s.processed && !s.idlePlayed && s.renderProbeCount < 90) {
                ++s.renderProbeCount;
                if(s.renderProbeCount % 3 == 1) {
                    Obj* avatar=nullptr;
                    auto avatarError=BNM::TryInvoke([&]{avatar=getAvatar[card]();});
                    bool rendered=false;
                    bool staticCardFace=false;
                    if(!avatarError.IsValid() && avatar && BNM::IsA(avatar,avatarClass)) {
                        Obj* avatarGO=nullptr;
                        auto goError=BNM::TryInvoke([&]{avatarGO=getAvatarObject[avatar]();});
                        if(!goError.IsValid() && avatarGO) {
                            rendered=Pick3NativeAvatarRender::AvatarReady(
                                s.option,avatarGO,Target(s.parentRoot),s.motion==MotionMode::Environment);
                        }
                    }
                    // A non-fighter may legitimately only provide in-hand art.
                    // Wait for an actual avatar before considering that native
                    // static source. Fighters/environments remain blank on fail.
                    if(!rendered && s.motion==MotionMode::Other && s.renderProbeCount >= 31) {
                        Obj* hand=nullptr;
                        Obj* handGO=nullptr;
                        Obj* sprite=nullptr;
                        if(getInHandSpriteRenderer.IsValid()) {
                            auto handError=BNM::TryInvoke([&]{hand=getInHandSpriteRenderer[card]();});
                            if(!handError.IsValid() && hand && BNM::IsA(hand,spriteRendererClass) &&
                               getSpriteRendererObject.IsValid() && getHandSprite.IsValid()) {
                                auto spriteError=BNM::TryInvoke([&]{
                                    handGO=getSpriteRendererObject[hand]();
                                    sprite=getHandSprite[hand]();
                                });
                                if(spriteError.IsValid()) {handGO=nullptr;sprite=nullptr;}
                            }
                        }
                        if(handGO && sprite) {
                            rendered=Pick3NativeAvatarRender::AvatarReady(s.option,handGO,
                                       Target(s.parentRoot),false);
                            staticCardFace=rendered;
                        }
                    }
                    if(rendered) {
                        s.idlePlayed=true;
                        LOG_INFO("CustomComponents/Pick3/AVATAR_LOAD: VISUAL_READY option=%d guid=%d mode=%s probes=%d",
                                 s.option+1,s.guid,staticCardFace?"game-inhand-static":"inspection-avatar",s.renderProbeCount);
                    } else if(s.renderProbeCount>=88) {
                        LOG_ERR("CustomComponents/Pick3/AVATAR_LOAD: VISUAL_UNAVAILABLE option=%d guid=%d attempts=%d; blank (never show card frame)",
                                s.option+1,s.guid,s.renderProbeCount);
                    }
                }
            }
        }
        Pick3NativeAvatarRender::Tick();
    }
}
