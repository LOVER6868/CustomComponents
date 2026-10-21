#pragma once
// PickFromThree: opt-in-independent, bounded UnityFS asset loading.
// No scene-wide Unity object enumeration, card factory, prefab instantiation,
// or async callbacks. All calls run on the frontend/Unity main thread.
// Bundles, sprites and textures are rooted for the process lifetime.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <algorithm>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "presentation/detail/Pick3BundledUnityFS.hpp"
#include "presentation/detail/Pick3NativeAvatarLoad.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3LocalAssetBundles {
    inline std::uint64_t ProbeClockMs() {
        struct timespec now{};
        if (::clock_gettime(CLOCK_MONOTONIC,&now)!=0) return 0;
        return static_cast<std::uint64_t>(now.tv_sec)*1000u+
               static_cast<std::uint64_t>(now.tv_nsec)/1000000u;
    }
    using Obj=BNM::IL2CPP::Il2CppObject;
    using Slot=Pick3BundledUnityFS::BundleSlot;
    struct Rect {float x,y,width,height;};
    struct Vec2 {float x,y;};
    struct Item { Obj* object=nullptr; std::uint32_t root=0; bool attempted=false; };
    inline Item bundles[static_cast<int>(Slot::Count)]{};
    inline Item sprites[7]{}; // grass, rim, sun, brain, attack, heart, button
    inline Item textures[7]{}; // only used if a bundle exposes Texture2D, not Sprite
    inline Item colors{};
    inline bool initialized=false, available=false, pathReady=false;
    // Fail closed after the 2026-10-19 UnityMain crash. A newly installed
    // config that omits this section must NOT enable native bundle calls.
    inline bool directBundleLoadingEnabled=false;
    // 0 disabled, 1 extract/check files, 2 open Circle bundle only,
    // 3 load/display just original grass Sprite, 4 full resolution (UNTESTED).
    // Do not combine probe 3 with other bundle or class-color experiments.
    inline int probeMode=1;
    // Full mode remains opt-in. If a platform-native API segfaults, BNM's
    // managed exception wrapper CANNOT catch it. AssetIndexLimit is a strict
    // kill-switch for isolating a crash without changing source code.
    inline int assetIndexLimit=6;
    inline bool textureFallbackEnabled=false;
    inline bool loadNativeColorScheme=false;
    inline bool enableSplitRim=true;
    inline bool enableButtonSlicing=false;
    inline void SetLoadNativeColorScheme(bool value){loadNativeColorScheme=value;}
    inline void SetEnableSplitRim(bool value){enableSplitRim=value;}
    inline void SetEnableButtonSlicing(bool value){enableButtonSlicing=value;}
    inline void SetAssetIndexLimit(int value){assetIndexLimit=std::clamp(value,0,6);}
    inline void SetTextureFallbackEnabled(bool value){textureFallbackEnabled=value;}
    inline void SetEnabled(bool value){
        directBundleLoadingEnabled=value;
        LOG_INFO("CustomComponents/Pick3/UNITYFS: CONFIG enabled=%d probeMode=%d", value?1:0,probeMode);
    }
    inline void SetProbeMode(int value){probeMode=std::max(0,std::min(value,4));}
    inline std::string diskDirectory;
    inline BNM::Class assetBundleClass,spriteClass,textureClass,schemeClass;
    inline BNM::Method<Obj*> loadFromFile,loadAsset,spriteCreate;
    inline BNM::Method<int> textureWidth,textureHeight;
    inline BNM::Method<void> unload;
    inline std::string BundleFileName(Slot slot) {
        static const char* names[]={"circle","sun","heart","attack","brain","button","colors"};
        int index=static_cast<int>(slot);
        return index>=0 && index<7 ? names[index] : "invalid";
    }
    inline bool ValidRoot(Obj* object,Item& item) {
        if(!object)return false;
        const auto handle=Pick3NativeAvatarLoad::Root(object);
        if(!handle)return false;
        item.object=object;item.root=handle;
        return true;
    }
    inline void Initialize() {
        if(initialized)return;
        initialized=true;
        assetBundleClass=BNM::Class("UnityEngine","AssetBundle");
        spriteClass=BNM::Class("UnityEngine","Sprite");
        textureClass=BNM::Class("UnityEngine","Texture2D");
        schemeClass=BNM::Class("PvZCards.Game","ScriptableObjectColorScheme");
        if(assetBundleClass.IsValid()) {
            loadFromFile=assetBundleClass.GetMethod("LoadFromFile",1).cast<Obj*>();
            loadAsset=assetBundleClass.GetMethod("LoadAsset",2).cast<Obj*>();
            unload=assetBundleClass.GetMethod("Unload",1).cast<void>();
        }
        if(spriteClass.IsValid()) spriteCreate=spriteClass.GetMethod("Create",3).cast<Obj*>();
        if(textureClass.IsValid()) {
            textureWidth=textureClass.GetMethod("get_width",0).cast<int>();
            textureHeight=textureClass.GetMethod("get_height",0).cast<int>();
        }
        available=loadFromFile.IsValid()&&loadAsset.IsValid()&&spriteClass.IsValid();
        LOG_INFO("CustomComponents/Pick3/UNITYFS: API ready=%d loadFile=%d loadAsset=%d sprite=%d",
            available?1:0,loadFromFile.IsValid(),loadAsset.IsValid(),spriteClass.IsValid());
        if(!available)return;
        LOG_INFO("CustomComponents/Pick3/UNITYFS: OPTIONS assetIndexLimit=%d textureFallback=%d nativeColors=%d splitRim=%d buttonSlicing=%d",
                 assetIndexLimit,textureFallbackEnabled,loadNativeColorScheme,
                 enableSplitRim,enableButtonSlicing);
        // Deliberately do NOT invoke UnityEngine.Application.get_persistentDataPath.
        // In the failing build, the last log was API_READY immediately before
        // that native invocation and a SIGSEGV at 0x38. Sprig already uses the
        // package's private files directory. Restrict this test to that known
        // location; if it is absent, return safely without a Unity call.
        const std::string base="/data/user/0/com.ea.gp.pvzheroes/files";
        LOG_INFO("CustomComponents/Pick3/UNITYFS: PATH_BEGIN");
        struct stat st{};
        if(::stat(base.c_str(),&st)!=0 || !S_ISDIR(st.st_mode)){
            LOG_INFO("CustomComponents/Pick3/UNITYFS: PATH_FAILED reason=missing-private-files");
            return;
        }
        const std::string parent=base+"/sprig";
        diskDirectory=parent+"/pick3-native-bundles";
        if((::mkdir(parent.c_str(),0700)!=0 && errno!=EEXIST) ||
           (::mkdir(diskDirectory.c_str(),0700)!=0 && errno!=EEXIST)){
            LOG_INFO("CustomComponents/Pick3/UNITYFS: PATH_FAILED reason=mkdir errno=%d",errno);
            return;
        }
        pathReady=true;
        LOG_INFO("CustomComponents/Pick3/UNITYFS: PATH_READY");
    }
    inline bool EnsureOnDisk(Slot slot,std::string& path) {
        if(!pathReady)return false;
        const auto blob=Pick3BundledUnityFS::Get(slot);
        if(!blob.data||!blob.size||blob.size>4*1024*1024)return false;
        path=diskDirectory+"/"+BundleFileName(slot)+".unityfs";
        // Verify all bytes, not just size, so an APK update cannot reuse a stale
        // bundle from an earlier release. Write atomically when changed.
        bool same=false;
        if(FILE* f=std::fopen(path.c_str(),"rb")) {
            std::uint8_t buffer[4096];std::size_t pos=0;same=true;
            while(pos<blob.size) {
                const std::size_t count=std::min(sizeof(buffer),blob.size-pos);
                if(std::fread(buffer,1,count,f)!=count ||
                   std::memcmp(buffer,blob.data+pos,count)!=0){same=false;break;}
                pos+=count;
            }
            if(same && std::fgetc(f)!=EOF)same=false;
            std::fclose(f);
        }
        if(same)return true;
        const std::string temp=path+".tmp";
        FILE* f=std::fopen(temp.c_str(),"wb");if(!f)return false;
        const bool written=std::fwrite(blob.data,1,blob.size,f)==blob.size;
        const bool closed=std::fclose(f)==0;
        if(!written||!closed||std::rename(temp.c_str(),path.c_str())!=0){
            std::remove(temp.c_str());return false;
        }
        return true;
    }
    inline Obj* GetBundle(Slot slot) {
        if(!directBundleLoadingEnabled || probeMode==0)return nullptr;
        // Stages 2 and 3 must NEVER open any bundle except Circle.
        if((probeMode==2 || probeMode==3) && slot!=Slot::Circle)return nullptr;
        Initialize();
        const int idx=static_cast<int>(slot);
        if(idx<0||idx>=static_cast<int>(Slot::Count)||!available)return nullptr;
        Item& item=bundles[idx];
        if(item.attempted)return item.object;
        item.attempted=true;
        std::string path;
        const auto fileBegin=ProbeClockMs();
        LOG_INFO("CustomComponents/Pick3/UNITYFS: FILE_BEGIN name=%s mode=%d",BundleFileName(slot).c_str(),probeMode);
        if(!EnsureOnDisk(slot,path)){
            LOG_INFO("CustomComponents/Pick3/UNITYFS: FILE_FAILED name=%s",BundleFileName(slot).c_str());
            return nullptr;
        }
        LOG_INFO("CustomComponents/Pick3/UNITYFS: FILE_READY name=%s elapsedMs=%llu",
                 BundleFileName(slot).c_str(),static_cast<unsigned long long>(ProbeClockMs()-fileBegin));
        if(probeMode==1)return nullptr; // Disk-only verification; no native load.
        const auto loadBegin=ProbeClockMs();
        LOG_INFO("CustomComponents/Pick3/UNITYFS: LOAD_BEGIN name=%s",BundleFileName(slot).c_str());
        Obj* object=nullptr;
        auto error=BNM::TryInvoke([&]{object=loadFromFile(BNM::CreateMonoString(path));});
        if(error.IsValid()||!ValidRoot(object,item)){
            LOG_INFO("CustomComponents/Pick3/UNITYFS: LOAD_FAILED name=%s exception=%d",BundleFileName(slot).c_str(),error.IsValid());
            return nullptr;
        }
        LOG_INFO("CustomComponents/Pick3/UNITYFS: BUNDLE_READY name=%s elapsedMs=%llu",
                 BundleFileName(slot).c_str(),static_cast<unsigned long long>(ProbeClockMs()-loadBegin));
        return item.object;
    }
    inline Obj* TryAsset(Slot slot,const char* name, BNM::Class type) {
        Obj* bundle=GetBundle(slot);
        if(probeMode<3||!bundle||!type.IsValid()||!name)return nullptr;
        Obj* result=nullptr;
        const auto assetBegin=ProbeClockMs();
        // Checkpoints intentionally flank the risky managed invocation. If a
        // native SIGSEGV occurs, the final checkpoint identifies that call.
        LOG_INFO("CustomComponents/Pick3/UNITYFS: ASSET_BEGIN bundle=%s asset=%s probe=%d",
                 BundleFileName(slot).c_str(),name,probeMode);
        auto error=BNM::TryInvoke([&]{result=loadAsset[bundle](BNM::CreateMonoString(name),type.GetMonoType());});
        LOG_INFO("CustomComponents/Pick3/UNITYFS: ASSET_END bundle=%s found=%d exception=%d elapsedMs=%llu",
                 BundleFileName(slot).c_str(),result?1:0,error.IsValid()?1:0,
                 static_cast<unsigned long long>(ProbeClockMs()-assetBegin));
        if(error.IsValid()) return nullptr;
        return result;
    }
    // Slot names are explicit; attempts are one-shot. A missing Sprite can use
    // the original Unity Texture2D (NOT PNG decoding) with Sprite.Create.
    inline Obj* Sprite(int index) {
        if(index<0||index>=7||!directBundleLoadingEnabled||probeMode==0)return nullptr;
        if(probeMode>=4 && index>assetIndexLimit)return nullptr;
        static constexpr Slot which[]={Slot::Circle,Slot::Circle,Slot::Sun,Slot::Brain,
                                      Slot::Attack,Slot::Heart,Slot::Button};
        if(probeMode<3){ (void)GetBundle(which[index]); return nullptr; }
        // Stage 3 is a *single asset* experiment; do not ask Unity for the
        // native ring, cost/stat icons, button, or colors at this stage.
        if(probeMode==3 && index!=0)return nullptr;
        Item& item=sprites[index];if(item.attempted)return item.object;
        LOG_INFO("CustomComponents/Pick3/UNITYFS: SPRITE_BEGIN index=%d probe=%d",index,probeMode);
        item.attempted=true;
        static constexpr const char* paths[]={
            "assets/ingameart/sprites/meta/fullscreenview/frame_circle_grass.png",
            "assets/ingameart/sprites/meta/miniinspectionview/smallinspectionframe.png",
            "assets/ingameart/sprites/bases_frames/inhnd_sun_gb.png",
            "assets/ingameart/sprites/boards/ui icons/ui_brain.png",
            "assets/ingameart/sprites/bases_frames/attack.png",
            "assets/ingameart/sprites/bases_frames/heart.png",
            "assets/ingameart/sprites/meta/buttons/button_tint.png"};
        static constexpr const char* shortNames[]={
            "frame_circle_grass","smallinspectionframe","inhnd_sun_gb","UI_brain",
            "attack","heart","button_tint"};
        // In a UnityFS bundle, a sprite can be a subasset of a texture with
        // the same path, hence both fullname and short-name attempts.
        Obj* sprite=TryAsset(which[index],paths[index],spriteClass);
        if(!sprite)sprite=TryAsset(which[index],shortNames[index],spriteClass);
        bool fromTexture=false;
        // No fallback Texture2D load or Sprite.Create in probe 3: those are
        // additional managed calls and would hide which operation crashed.
        // A Sprite miss stays on the verified original PNG path for now.
        if(!sprite && probeMode>=4 && textureFallbackEnabled && textureClass.IsValid() && spriteCreate.IsValid() &&
           textureWidth.IsValid() && textureHeight.IsValid()) {
            Obj* tex=TryAsset(which[index],paths[index],textureClass);
            if(!tex)tex=TryAsset(which[index],shortNames[index],textureClass);
            if(tex){
                int w=0,h=0;
                const auto err=BNM::TryInvoke([&]{w=textureWidth[tex]();h=textureHeight[tex]();});
                if(!err.IsValid() && w>0&&h>0&&w<=8192&&h<=8192){
                    Obj* made=nullptr;
                    LOG_INFO("CustomComponents/Pick3/UNITYFS: SPRITE_CREATE_BEGIN index=%d",index);
                    auto makeErr=BNM::TryInvoke([&]{made=spriteCreate(tex,
                        Rect{0,0,static_cast<float>(w),static_cast<float>(h)},Vec2{.5f,.5f});});
                    LOG_INFO("CustomComponents/Pick3/UNITYFS: SPRITE_CREATE_END index=%d created=%d exception=%d",
                             index,made?1:0,makeErr.IsValid()?1:0);
                    if(!makeErr.IsValid()&&made&&ValidRoot(tex,textures[index])){
                        sprite=made;fromTexture=true;
                    }
                }
            }
        }
        if(sprite && ValidRoot(sprite,item)){
            LOG_INFO("CustomComponents/Pick3/UNITYFS: SPRITE_READY index=%d name=%s source=%s",
                index,shortNames[index],fromTexture?"native-texture":"native-sprite");
            return item.object;
        }
        LOG_INFO("CustomComponents/Pick3/UNITYFS: SPRITE_MISSING index=%d name=%s",index,shortNames[index]);
        return nullptr;
    }
    inline Obj* ColorScheme() {
        if(!directBundleLoadingEnabled||probeMode==0)return nullptr;
        if(!loadNativeColorScheme || assetIndexLimit<6)return nullptr;
        // Probe 3 is reserved for the original grass Sprite alone; mode 4 is opt-in.
        if(probeMode<4){if(probeMode==1)(void)GetBundle(Slot::Colors);return nullptr;}
        if(colors.attempted)return colors.object;
        LOG_INFO("CustomComponents/Pick3/UNITYFS: COLOR_SCHEME_BEGIN");
        colors.attempted=true;
        Obj* object=TryAsset(Slot::Colors,"assets/ship/frameassets/defaultframecolors.asset",schemeClass);
        if(!object)object=TryAsset(Slot::Colors,"DefaultFrameColors",schemeClass);
        if(object) ValidRoot(object,colors);
        LOG_INFO("CustomComponents/Pick3/UNITYFS: COLOR_SCHEME_READY=%d",colors.object?1:0);
        return colors.object;
    }
}
