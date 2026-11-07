#pragma once
namespace CustomComponents::Core::PresentationRefresh {
struct Choice {const void* object=nullptr;int state=-1,phase=-1,effect=-1,group=-1;};
inline bool NeedsRefresh(bool ownPrimary,const Choice& old,const Choice& next) {
    return ownPrimary && next.object && next.phase==0 &&
        (old.object!=next.object || old.state!=6 || old.phase!=0 || old.effect!=next.effect || old.group!=next.group);
}
struct FrameGate {
    int last=-1;
    bool Enter(int frame){if(frame==last)return false;last=frame;return true;}
};
// A same-state holder can be current before its stock target presentation has
// populated an offer. Rebind a missing offer at most twice per exact choice.
struct MissingOfferRefresh {
    Choice choice{};int stable=0,attempts=0,nextFrame=0,lastFrame=-1;
    bool Enter(bool eligible,int frame,const Choice& next) {
        if(choice.object!=next.object || choice.effect!=next.effect || choice.group!=next.group || choice.phase!=next.phase){choice=next;stable=attempts=nextFrame=0;lastFrame=-1;}
        if(frame==lastFrame)return false;lastFrame=frame;
        if(!eligible){stable=0;return false;}
        if(++stable<2 || attempts>=2 || frame<nextFrame)return false;
        ++attempts;nextFrame=frame+30;return true;
    }
};
}
