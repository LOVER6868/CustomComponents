#pragma once
#include <cmath>
namespace CustomComponents::Level::Gesture {
inline constexpr float MinimumLeg=.025f,MinimumTravel=.10f,MinimumSpeed=.20f;
inline constexpr double Window=.55,Cooldown=.45;
inline constexpr int Reversals=2;
struct Detector {
    bool seeded=false;float x=0,y=0,extreme=0,travel=0;int axis=-1,direction=0,reversals=0;double started=0,last=0,blockedUntil=0;
    void Reset(){seeded=false;axis=-1;direction=reversals=0;travel=0;}
    bool Sample(float nx,float ny,double now) {
        if(now<blockedUntil){Reset();return false;}
        if(!seeded){seeded=true;x=nx;y=ny;last=started=now;return false;}
        const float dx=nx-x,dy=ny-y;const double dt=now-last;
        if(dt<=0 || dt>.2 || now-started>Window){Reset();return Sample(nx,ny,now);}
        if(axis<0){if(std::max(std::abs(dx),std::abs(dy))<.004f)return false;axis=std::abs(dx)>=std::abs(dy)?0:1;extreme=axis?y:x;}
        float pos=axis?ny:nx;float movement=axis?dy:dx;travel+=std::abs(movement);
        if(std::abs(movement)/dt<MinimumSpeed && std::abs(movement)>.004f){Reset();return Sample(nx,ny,now);}
        int sign=movement>=0?1:-1;
        if(direction==0){direction=sign;extreme=pos;}
        else if(sign==direction){if((direction>0 && pos>extreme) || (direction<0 && pos<extreme))extreme=pos;}
        else if(std::abs(pos-extreme)>=MinimumLeg){++reversals;direction=sign;extreme=pos;}
        x=nx;y=ny;last=now;
        if(reversals>=Reversals && travel>=MinimumTravel){blockedUntil=now+Cooldown;Reset();return true;}return false;
    }
};
}
