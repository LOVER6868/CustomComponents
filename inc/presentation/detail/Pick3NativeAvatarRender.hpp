#pragma once
// Isolated native avatar -> three independent 512x512 RenderTextures -> UGUI RawImages.
// Native UGUI artwork is unmasked and may extend beyond the class circle.
// ScreenSpaceOverlay popup never waits for these resources. Only stock avatars on
// dedicated portrait layers 29-31 are captured; if APIs/objects are missing, stock face art stays.
// Unity Camera setters used here are registered engine icalls but not included
// in this game's stripped C# metadata. Verify every icall before enabling.
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3NativeAvatarRender {
    using Obj=BNM::IL2CPP::Il2CppObject;
    struct Vec3{float x,y,z;};
    struct Bounds{Vec3 center,extents;};
    struct Color{float r,g,b,a;};
    constexpr int kOptions=3;
    // Unity cameras use layer masks, not per-object identity. Give each slot
    // a distinct layer so no camera can render a sibling avatar.
    constexpr int kFirstPortraitLayer=29;
    inline int PortraitLayer(int option){return kFirstPortraitLayer+option;}
    constexpr int kTexWidth=512,kTexHeight=512;
    struct Stage{
        std::uint32_t cameraRoot=0,rtRoot=0,rawRoot=0;
        bool prepared=false,visible=false,reported=false,environment=false;
        int paintedFrames=0;
        int category=0; // 0=fighter, 1=environment, 2=trick/other
        std::chrono::steady_clock::time_point lastCardPaint{};
        std::uint32_t ticket=0;
    };
    inline Stage stages[kOptions]{};
    inline bool ready=false,requested=false;
    // Camera framing (config.ini, percents/100). Preserves game-owned CardView
    // transforms and never overwrites the prefab's authored local scale.
    inline float cameraZoom=1.f,portraitPadding=1.22f;
    // Match the enlarged square RawImage viewport without enlarging native art.
    // Expands the actual visible camera field; does not eliminate finite RT edges.
    inline float artworkViewportExpansion=2.5f;
    inline float minOrtho=.20f,maxOrtho=12.f;
    inline float offsetX=0.f,offsetY=0.f;
    // Long environment art can have its visible focal point far above the
    // geometric center. Reframe the environment camera independently.
    inline float environmentZoom=1.f,environmentCameraYOffset=0.f;
    inline bool dominantRenderer=true;
    // Stock-inspired camera baseline: stock inspection attaches authored CardViews
    // to stable anchors instead of deriving the entire displayed scale from
    // volatile particle/renderer bounds. Our three off-screen cameras cannot
    // reuse the stock scene camera, so these are configurable baseline frames.
    // Expansion occurs only when measured artwork would otherwise be clipped.
    inline bool stableInspectionFraming=true;
    inline float fighterBaselineOrtho=7.0f,environmentBaselineOrtho=9.0f,otherBaselineOrtho=7.5f;
    inline float safetyOverscan=1.20f;
    inline int renderEveryNTicks=1;
    inline int renderBudgetFps=90; // absolute global safety limit
    inline int fighterFps=30,environmentFps=24,otherFps=24;
    inline std::chrono::steady_clock::time_point lastPaint{};
    inline std::uint32_t paintCalls=0,paintSlot=0;
    inline std::uint32_t renderTickCounter=0;
    inline void ConfigureRenderCadence(int ticks){
        renderEveryNTicks=std::clamp(ticks,1,12);
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: CONFIG renderEveryNTicks=%d (native experiment only)",renderEveryNTicks);
    }
    inline void ConfigureRenderBudgetFps(int fps){renderBudgetFps=std::clamp(fps,1,240);}
    inline void ConfigurePerCardFps(int fighter,int environment,int other){
        fighterFps=std::clamp(fighter,1,90);
        environmentFps=std::clamp(environment,0,90);
        otherFps=std::clamp(other,0,90);
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: PER_CARD fighter=%d environment=%d other=%d globalMax=%d",
                 fighterFps,environmentFps,otherFps,renderBudgetFps);
    }
    inline void ConfigureFraming(int zoomPercent,int paddingPercent,int minHundredths,
                                 int maxHundredths,int xOffsetPercent,int yOffsetPercent,
                                 bool dominant,int envZoomPercent=100,
                                 int envCameraYOffsetPercent=0,
                                 bool useStableInspectionFraming=true,
                                 int fighterOrthoHundredths=700,
                                 int environmentOrthoHundredths=900,
                                 int otherOrthoHundredths=750,
                                 int safetyOverscanPercent=120) {
        cameraZoom=std::clamp(zoomPercent/100.f,.40f,2.5f);
        portraitPadding=std::clamp(paddingPercent/100.f,1.0f,2.f);
        minOrtho=std::clamp(minHundredths/100.f,.03f,5.f);
        maxOrtho=std::clamp(maxHundredths/100.f,1.f,50.f);
        offsetX=std::clamp(xOffsetPercent/100.f,-.5f,.5f);
        offsetY=std::clamp(yOffsetPercent/100.f,-.5f,.5f);
        environmentZoom=std::clamp(envZoomPercent/100.f,.4f,4.f);
        environmentCameraYOffset=std::clamp(envCameraYOffsetPercent/100.f,-1.f,1.f);
        dominantRenderer=dominant;
        stableInspectionFraming=useStableInspectionFraming;
        fighterBaselineOrtho=std::clamp(fighterOrthoHundredths/100.f,1.f,30.f);
        environmentBaselineOrtho=std::clamp(environmentOrthoHundredths/100.f,1.f,30.f);
        otherBaselineOrtho=std::clamp(otherOrthoHundredths/100.f,1.f,30.f);
        safetyOverscan=std::clamp(safetyOverscanPercent/100.f,1.05f,2.f);
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: FRAMING mode=%s baseline=%.2f,%.2f,%.2f overscan=%.2f viewport=%.2f",
                 stableInspectionFraming?"inspection-baseline":"legacy-bounds",
                 fighterBaselineOrtho,environmentBaselineOrtho,otherBaselineOrtho,safetyOverscan,artworkViewportExpansion);
    }
    inline BNM::Class cameraClass, renderTextureClass, rawClass, gameObjectClass, transformClass,
                      componentClass, rendererClass, behaviourClass;
    inline BNM::Method<Obj*> getTransform,getComponent,getChild,getGameObject;
    inline BNM::Method<int> getChildCount;
    inline BNM::Method<Bounds> getBounds;
    inline BNM::Method<void> setLayer,setPosition,setOrthoSize,setTexture,setEnabled,destroyObject;
    inline BNM::Method<Obj*> addComponent;
    inline BNM::Method<void> setRaycast;
    using ResolveIcall=void* (*)(const char*);
    using CamBool=void(*)(Obj*,bool);
    using CamInt=void(*)(Obj*,int);
    using CamRt=void(*)(Obj*,Obj*);
    using CamColor=void(*)(Obj*,Color*);
    using CamRender=void(*)(Obj*);
    inline CamBool setOrthographic=nullptr;
    inline CamInt setCullingMask=nullptr,setClearFlags=nullptr;
    inline CamRt setTargetTexture=nullptr;
    inline CamColor setBackgroundColor=nullptr;
    inline CamRender render=nullptr;

    inline std::uint32_t Root(Obj* obj) {
        return obj && Core::SceneViewContext::newGCHandle ?
               Core::SceneViewContext::newGCHandle(obj,false):0;
    }
    inline Obj* Target(std::uint32_t root) {
        return root && Core::SceneViewContext::getGCHandle ? Core::SceneViewContext::getGCHandle(root):nullptr;
    }
    inline void Free(std::uint32_t& root) {
        if(root && Core::SceneViewContext::freeGCHandle) Core::SceneViewContext::freeGCHandle(root);
        root=0;
    }
    inline bool Initialize(bool enable) {
        requested=enable;ready=false;renderTickCounter=0;paintSlot=0;paintCalls=0;lastPaint={};
        if(!enable) return true;
        cameraClass=BNM::Class("UnityEngine","Camera");
        renderTextureClass=BNM::Class("UnityEngine","RenderTexture");
        rawClass=BNM::Class("UnityEngine.UI","RawImage");
        gameObjectClass=BNM::Class("UnityEngine","GameObject");
        transformClass=BNM::Class("UnityEngine","Transform");
        componentClass=BNM::Class("UnityEngine","Component");
        rendererClass=BNM::Class("UnityEngine","Renderer");
        behaviourClass=BNM::Class("UnityEngine","Behaviour");
        if(gameObjectClass.IsValid()) {
            getTransform=gameObjectClass.GetMethod("get_transform",0).cast<Obj*>();
            getComponent=gameObjectClass.GetMethod("GetComponent",1).cast<Obj*>();
            addComponent=gameObjectClass.GetMethod("AddComponent",1).cast<Obj*>();
            setLayer=gameObjectClass.GetMethod("set_layer",1).cast<void>();
        }
        if(transformClass.IsValid()) {
            getChildCount=transformClass.GetMethod("get_childCount",0).cast<int>();
            getChild=transformClass.GetMethod("GetChild",1).cast<Obj*>();
            setPosition=transformClass.GetMethod("set_position",1).cast<void>();
        }
        if(componentClass.IsValid()) getGameObject=componentClass.GetMethod("get_gameObject",0).cast<Obj*>();
        if(rendererClass.IsValid()) getBounds=rendererClass.GetMethod("get_bounds",0).cast<Bounds>();
        if(cameraClass.IsValid()) setOrthoSize=cameraClass.GetMethod("set_orthographicSize",1).cast<void>();
        if(rawClass.IsValid()) setTexture=rawClass.GetMethod("set_texture",1).cast<void>();
        if(behaviourClass.IsValid()) setEnabled=behaviourClass.GetMethod("set_enabled",1).cast<void>();
        auto graphic=BNM::Class("UnityEngine.UI","Graphic");
        if(graphic.IsValid()) setRaycast=graphic.GetMethod("set_raycastTarget",1).cast<void>();
        destroyObject=BNM::Class("UnityEngine","Object").GetMethod("Destroy",1).cast<void>();
        auto* fn=reinterpret_cast<ResolveIcall>(dlsym(RTLD_DEFAULT,"il2cpp_resolve_icall"));
        if(!fn) {
            // Some Android linkers load Unity's IL2CPP handle RTLD_LOCAL.
            // Resolve by the existing soname; don't assume RTLD_DEFAULT.
            void* lib=dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);
            if(lib) {
                fn=reinterpret_cast<ResolveIcall>(dlsym(lib,"il2cpp_resolve_icall"));
                dlclose(lib);
            }
        }
        if(fn) {
            setOrthographic=reinterpret_cast<CamBool>(fn("UnityEngine.Camera::set_orthographic"));
            setCullingMask=reinterpret_cast<CamInt>(fn("UnityEngine.Camera::set_cullingMask"));
            setClearFlags=reinterpret_cast<CamInt>(fn("UnityEngine.Camera::set_clearFlags"));
            setTargetTexture=reinterpret_cast<CamRt>(fn("UnityEngine.Camera::set_targetTexture"));
            setBackgroundColor=reinterpret_cast<CamColor>(fn("UnityEngine.Camera::set_backgroundColor_Injected"));
            render=reinterpret_cast<CamRender>(fn("UnityEngine.Camera::Render"));
        }
        ready=cameraClass.IsValid() && renderTextureClass.IsValid() && rawClass.IsValid() &&
              gameObjectClass.IsValid() && transformClass.IsValid() && componentClass.IsValid() &&
              rendererClass.IsValid() && behaviourClass.IsValid() && getTransform.IsValid() &&
              getComponent.IsValid() && addComponent.IsValid() && setLayer.IsValid() &&
              getChildCount.IsValid() && getChild.IsValid() && getGameObject.IsValid() &&
              setPosition.IsValid() && getBounds.IsValid() && setOrthoSize.IsValid() &&
              setTexture.IsValid() && setEnabled.IsValid() && destroyObject.IsValid() &&
              setOrthographic && setCullingMask && setClearFlags && setTargetTexture &&
              setBackgroundColor && render && Core::SceneViewContext::newGCHandle &&
              Core::SceneViewContext::freeGCHandle && Core::SceneViewContext::getGCHandle;
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: INIT requested=%d ready=%d icalls=%d camera=%d texture=%d raw=%d",
             requested?1:0,ready?1:0,
             setOrthographic&&setCullingMask&&setClearFlags&&setTargetTexture&&setBackgroundColor&&render?1:0,
             cameraClass.IsValid()?1:0,renderTextureClass.IsValid()?1:0,rawClass.IsValid()?1:0);
        return true; // No avatar render error is allowed to block the popup.
    }
    inline void Stop(int option,std::uint32_t ownerTicket=0) {
        if(option<0||option>=kOptions)return;
        Stage& s=stages[option];
        if(ownerTicket && s.ticket!=ownerTicket)return;
        // Detach UI before retiring the RenderTexture. Unity Destroy is deferred,
        // and leaving a RawImage bound can expose a dead texture next frame.
        Obj* raw=Target(s.rawRoot);
        if(raw) {
            BNM::TryInvoke([&]{
                if(setEnabled.IsValid()) setEnabled[raw](false);
                if(setTexture.IsValid()) setTexture[raw](nullptr);
            });
        }
        Obj* camGO=Target(s.cameraRoot);
        Obj* rt=Target(s.rtRoot);
        if(camGO) {
            Obj* cam=nullptr;
            if(getComponent.IsValid())
                BNM::TryInvoke([&]{cam=getComponent[camGO](cameraClass.GetMonoType());});
            if(cam && setTargetTexture) BNM::TryInvoke([&]{setTargetTexture(cam,nullptr);});
            if(destroyObject.IsValid()) BNM::TryInvoke([&]{destroyObject(camGO);});
        }
        if(rt && destroyObject.IsValid()) BNM::TryInvoke([&]{destroyObject(rt);});
        Free(s.rawRoot);Free(s.cameraRoot);Free(s.rtRoot);s=Stage{};
    }
    inline void EndSession(){for(int i=0;i<kOptions;++i)Stop(i);}
    inline bool BeginStage(int option,Obj* /*parent*/,std::uint32_t ticket,int category=0){
        if(!ready||option<0||option>=kOptions)return false;
        Stop(option);
        Stage& s=stages[option];
        s.ticket=ticket;
        s.category=std::clamp(category,0,2);
        char name[50]{};std::snprintf(name,sizeof(name),"Sprig_Pick3_RT_Camera_%d",option+1);
        Obj* cameraGO=gameObjectClass.CreateNewObjectParameters(BNM::CreateMonoString(name));
        s.cameraRoot=Root(cameraGO);
        if(!s.cameraRoot)return false;
        Obj* cam=addComponent[cameraGO](cameraClass.GetMonoType());
        if(!cam){Stop(option);return false;}
        Obj* rt=renderTextureClass.CreateNewObjectParameters(kTexWidth,kTexHeight,16);
        s.rtRoot=Root(rt);
        if(!s.rtRoot){Stop(option);return false;}
        setTargetTexture(cam,rt);
        setOrthographic(cam,true);
        setCullingMask(cam,static_cast<int>(1u<<PortraitLayer(option)));
        setClearFlags(cam,2); // SolidColor = transparent black
        Color clear{0.f,0.f,0.f,0.f};setBackgroundColor(cam,&clear);
        setOrthoSize[cam](1.f);
        setEnabled[cam](false); // explicit Render() after avatar is ready
        s.prepared=true;
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: STAGE option=%d rt=%p",option+1,rt);
        return true;
    }
    inline void Relayer(Obj* go,int layer,int depth,Bounds& bounds,bool& found,std::array<Bounds,128>& samples,int& n){
        if(!go||depth>18)return;
        setLayer[go](layer);
        Obj* renderer=getComponent[go](rendererClass.GetMonoType());
        if(renderer&&getBounds.IsValid()){
            Bounds b{};
            auto ex=BNM::TryInvoke([&]{b=getBounds[renderer]();});
            if(!ex.IsValid() && std::isfinite(b.extents.x) && std::isfinite(b.extents.y) &&
               (b.extents.x>0.001f||b.extents.y>0.001f)){
                if(n<static_cast<int>(samples.size()))samples[n++]=b;
                if(!found){bounds=b;found=true;}
                else {
                    auto merge=[](float &center,float &extent,float newCenter,float newExtent){
                        const float lo=std::min(center-extent,newCenter-newExtent);
                        const float hi=std::max(center+extent,newCenter+newExtent);
                        center=(lo+hi)*.5f;
                        extent=(hi-lo)*.5f;
                    };
                    merge(bounds.center.x,bounds.extents.x,b.center.x,b.extents.x);
                    merge(bounds.center.y,bounds.extents.y,b.center.y,b.extents.y);
                    merge(bounds.center.z,bounds.extents.z,b.center.z,b.extents.z);
                }
            }
        }
        Obj* tr=getTransform[go]();
        if(!tr)return;
        int count=getChildCount[tr]();
        count=std::clamp(count,0,128);
        for(int i=0;i<count;++i){
            Obj* child=getChild[tr](i);
            Obj* childGO=child?getGameObject[child]():nullptr;
            Relayer(childGO,layer,depth+1,bounds,found,samples,n);
        }
    }
    inline bool AvatarReady(int option,Obj* avatarGO,Obj* parentGO,bool environment=false){
        if(!ready||option<0||option>=kOptions||!stages[option].prepared||!avatarGO)return false;
        Bounds b{};bool found=false;
        std::array<Bounds,128> samples{};int sampleCount=0;
        // Protect every renderer under the newly-created avatar, including
        // instantiated effect children. The parent root has no renderers.
        if(parentGO)setLayer[parentGO](PortraitLayer(option));
        Relayer(avatarGO,PortraitLayer(option),0,b,found,samples,sampleCount);
        if(!found){
            // This is usually temporary while the async prefab initializes.
            // Do not spam a native error every readiness poll.
            if(!stages[option].reported){
                stages[option].reported=true;
                LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: WAIT_RENDERERS option=%d",option+1);
            }
            return false;
        }
        Obj* camGO=Target(stages[option].cameraRoot);
        Obj* cam=camGO?getComponent[camGO](cameraClass.GetMonoType()):nullptr;
        Obj* tr=camGO?getTransform[camGO]():nullptr;
        if(!cam||!tr)return false;
        // Do not refit to the ENTIRE animated hierarchy every few frames.
        // Particle systems / oversized FX can make a Dragonfruit's body
        // appear tiny. Use the dominant visible 2D renderer (excluding
        // outliers 6x above the median extent), then hold camera fixed.
        if(dominantRenderer && sampleCount>1) {
            // Robust cluster fit: select a representative large sprite, then
            // merge only other sprites reasonably close to its center/scale.
            // This excludes transient far-away particles and tiny FX meshes.
            std::array<float,128> sizes{};
            for(int i=0;i<sampleCount;++i)
                sizes[i]=std::max(samples[i].extents.x,samples[i].extents.y);
            auto sorted=sizes;
            std::sort(sorted.begin(),sorted.begin()+sampleCount);
            const float reference=std::max(sorted[(sampleCount*3)/4],.04f);
            float best=-1.f;int bestIndex=-1;
            for(int i=0;i<sampleCount;++i){
                const float dimension=sizes[i];
                if(dimension<reference*.35f || dimension>reference*3.0f)continue;
                const float area=samples[i].extents.x*samples[i].extents.y;
                if(area>best){best=area;bestIndex=i;}
            }
            if(bestIndex>=0){
                const Bounds anchor=samples[bestIndex];
                b=anchor;
                for(int i=0;i<sampleCount;++i){
                    const Bounds &candidate=samples[i];
                    if(sizes[i]<reference*.25f || sizes[i]>reference*2.5f)continue;
                    if(std::abs(candidate.center.x-anchor.center.x)>reference*2.5f ||
                       std::abs(candidate.center.y-anchor.center.y)>reference*2.5f)continue;
                    // The anchor, not the growing merged bounds, defines the
                    // stable reference point for nearby sprite inclusion.
                    const float left=std::min(b.center.x-b.extents.x,candidate.center.x-candidate.extents.x);
                    const float right=std::max(b.center.x+b.extents.x,candidate.center.x+candidate.extents.x);
                    const float bottom=std::min(b.center.y-b.extents.y,candidate.center.y-candidate.extents.y);
                    const float top=std::max(b.center.y+b.extents.y,candidate.center.y+candidate.extents.y);
                    b.center.x=(left+right)*.5f;b.extents.x=(right-left)*.5f;
                    b.center.y=(bottom+top)*.5f;b.extents.y=(top-bottom)*.5f;
                }
            }
        }
        // Square portrait RT. Fit both dimensions BEFORE zoom/padding;
        // otherwise the narrow render texture clips wide/tall characters.
        const float fullExtent=std::max(b.extents.x*static_cast<float>(kTexHeight)/kTexWidth,b.extents.y);
        const float zoom=cameraZoom*(environment?environmentZoom:1.f);
        // Leave extra transparent overscan for animated extremities and FX.
        // This cannot guarantee that effects emitted outside the initial
        // renderer bounds will remain visible; never claim that it does.
        const float measuredFit=fullExtent*std::max(portraitPadding,1.45f)*artworkViewportExpansion/zoom;
        const float baseline=(environment?environmentBaselineOrtho:
                              stages[option].category==2?otherBaselineOrtho:fighterBaselineOrtho)/zoom;
        // The wide transparent UI layer is larger than the class plate;
        // the orthographic camera keeps a consistent base world-space extent.
        // Preserve renderer bounds as a clipping guard, NOT the default zoom.
        const float ortho=stableInspectionFraming?
            std::clamp(std::max(baseline,fullExtent*safetyOverscan/zoom),minOrtho,maxOrtho):
            std::clamp(measuredFit,minOrtho,maxOrtho);
        const float yOffset=offsetY+(environment?environmentCameraYOffset:0.f);
        const auto ex=BNM::TryInvoke([&]{
            setPosition[tr](Vec3{b.center.x+offsetX*fullExtent,b.center.y+yOffset*fullExtent,b.center.z-12.f});
            setOrthoSize[cam](ortho);
        });
        if(ex.IsValid())return false;
        stages[option].environment=environment;
        // Paint the first transparent-background frame before exposing the UI
        // texture. An uninitialized RawImage / RenderTexture may show a white
        // quad if card loading finishes between two render ticks.
        if(render) {
            const auto begin=std::chrono::steady_clock::now();
            render(cam);
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
            LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: FIRST_RENDER option=%d cpuUs=%lld",option+1,static_cast<long long>(elapsed));
        }
        stages[option].paintedFrames=1;
        stages[option].visible=true;
        LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: READY option=%d environment=%d ortho=%.3f bounds=%.3f,%.3f rendererCount=%d dominant=%d zoom=%.2f cameraYOffset=%.2f",option+1,environment?1:0,ortho,b.extents.x,b.extents.y,sampleCount,dominantRenderer?1:0,zoom,yOffset);
        return true;
    }
    inline Obj* Texture(int option){return ready&&option>=0&&option<kOptions&&
                                    stages[option].visible&&stages[option].paintedFrames>=3?
                                    Target(stages[option].rtRoot):nullptr;}
    inline bool IsEnvironment(int option){return option>=0&&option<kOptions&&
                                    stages[option].visible&&stages[option].environment;}
    inline bool AttachRaw(int option,Obj* uiGameObject){
        if(!ready||option<0||option>=kOptions||!uiGameObject)return false;
        Obj* raw=addComponent[uiGameObject](rawClass.GetMonoType());
        if(!raw)return false;
        if(setRaycast.IsValid())setRaycast[raw](false);
        // RawImage's default null texture renders Unity's *white* texture.
        // Leave the Graphic disabled until a real, first-rendered RT is ready.
        if(setEnabled.IsValid())setEnabled[raw](false);
        return true;
    }
    inline bool Bind(int option,Obj* uiGameObject){
        Obj* rt=Texture(option);
        if(!rt||!uiGameObject)return false;
        Obj* raw=getComponent[uiGameObject](rawClass.GetMonoType());
        if(!raw)return false;
        const auto ex=BNM::TryInvoke([&]{
            setTexture[raw](rt);
            if(setEnabled.IsValid())setEnabled[raw](true);
        });
        if(ex.IsValid())return false;
        // Root the UI component while the native RT is attached; Stop() unbinds
        // and releases this handle before destroying the target texture.
        Stage& stage=stages[option];
        Free(stage.rawRoot);
        stage.rawRoot=Root(raw);
        if(!stage.rawRoot) {
            BNM::TryInvoke([&]{
                if(setEnabled.IsValid())setEnabled[raw](false);
                setTexture[raw](nullptr);
            });
            return false;
        }
        return true;
    }
    inline void Tick(){
        if(!ready)return;
        if(++renderTickCounter%static_cast<std::uint32_t>(renderEveryNTicks)!=0)return;
        const auto now=std::chrono::steady_clock::now();
        // Previous implementation painted only ONE camera per Unity UI tick.
        // On a 30-Hz update loop, that capped three portraits around 10 FPS.
        // Let each independent stage paint when due. A rolling one-second
        // safety cap prevents unbounded work, without starving sibling cards.
        static auto windowStart=now;
        static int windowPaints=0;
        if(std::chrono::duration<double>(now-windowStart).count()>=1.0){
            windowStart=now;windowPaints=0;
        }
        for(int attempt=0;attempt<kOptions;++attempt){
            if(windowPaints>=renderBudgetFps)break;
            const int i=static_cast<int>(paintSlot++%kOptions);
            Stage& s=stages[i];if(!s.visible)continue;
            const int fps=s.category==1?environmentFps:(s.category==2?otherFps:fighterFps);
            if(s.paintedFrames>=3){
                if(fps==0)continue;
                if(s.lastCardPaint.time_since_epoch().count() &&
                   std::chrono::duration<double>(now-s.lastCardPaint).count()<1.0/fps)continue;
            }
            Obj* go=Target(s.cameraRoot);
            Obj* cam=go?getComponent[go](cameraClass.GetMonoType()):nullptr;
            if(!cam)continue;
            const auto begin=std::chrono::steady_clock::now();
            render(cam);
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
            s.lastCardPaint=lastPaint=std::chrono::steady_clock::now();
            s.paintedFrames=std::min(s.paintedFrames+1,8);
            ++windowPaints;
            if(++paintCalls%1800==0)
                LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: PERF option=%d type=%d cpuUs=%lld calls=%u cardFps=%d globalMax=%d",
                         i+1,s.category,static_cast<long long>(elapsed),paintCalls,fps,renderBudgetFps);
        }
    }
}
