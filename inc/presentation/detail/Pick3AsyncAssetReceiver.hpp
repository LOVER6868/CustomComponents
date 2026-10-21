#pragma once
// The PvZCards.ResourceLoader.LoadResourceAsync<CardFaceSpriteHolder> callback
// is an Action<CardFaceSpriteHolder>. BNM registers this managed receiver so
// that the game's own asynchronous resource loader can call native code
// without raw/unmanaged callback pointers or new hooks.
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Delegates.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Presentation::Pick3AsyncAssetReceiver {
    using Obj = BNM::IL2CPP::Il2CppObject;
    inline void (*deliver)(int option, int generation, Obj* asset) = nullptr;

    struct Receiver : Obj {
        int option = -1;
        int generation = -1;

        BNM_CustomClass(
            Receiver,
            BNM::CompileTimeClassBuilder("Sprig.Pick3", "AsyncCardFaceReceiver", "Assembly-CSharp.dll").Build(),
            BNM::CompileTimeClassBuilder("System", "Object", "mscorlib.dll").Build(),
            {}
        );

        void OnLoaded(Obj* asset) {
            if (deliver) deliver(option, generation, asset);
        }
        BNM_CustomMethod(
            OnLoaded, false, BNM::Defaults::Get<void>(), "OnLoaded",
            BNM::CompileTimeClassBuilder("PvZCards.CardFaceSprites", "CardFaceSpriteHolder", "Assembly-CSharp.dll").Build()
        );
    };
}
