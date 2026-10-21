#pragma once
// Crash isolation only. Device selector is read on each popup frame.
// 0=normal; 1=bypass per-frame popup UI; 2=bypass popup UI AND async services.
#include <cstdio>
#include "log.h"
namespace CustomComponents::Presentation::Pick3PostOpenIsolation {
    inline constexpr const char* kModeFile =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_postopen_isolation.txt";
    inline int lastAnnounced = -1;
    inline int ReadMode() {
        int value=0;
        if (FILE* f=std::fopen(kModeFile,"rb")) {
            if(std::fscanf(f,"%d",&value)!=1)value=0;
            std::fclose(f);
        }
        if(value<0 || value>2)value=0;
        if(value!=lastAnnounced){
            lastAnnounced=value;
            LOG_INFO("CustomComponents/Pick3/POSTOPEN_ISO: mode=%d (0=normal 1=skip-open-tick 2=skip-open-tick-and-async)",value);
        }
        return value;
    }
}
