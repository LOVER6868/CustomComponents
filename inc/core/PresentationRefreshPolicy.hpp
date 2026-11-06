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
}
