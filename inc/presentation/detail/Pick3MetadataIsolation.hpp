#pragma once
// Per-popup runtime isolation for the Pick3 metadata-to-UGUI path.
// Independent of customcomponents_pick3_hooks.txt and pick3_diag_mode.txt.
#include <cstdio>
#include "log.h"
namespace CustomComponents::Presentation::Pick3MetadataIsolation {
    inline constexpr const char* kModeFile=
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_metadata_isolation.txt";
    inline int mode=0;
    inline int ReadMode() {
        int requested=0;
        if (FILE* f=std::fopen(kModeFile,"rb")) {
            if (std::fscanf(f,"%d",&requested)!=1) requested=0;
            std::fclose(f);
        }
        if (requested<0 || requested>4) requested=0;
        mode=requested;
        LOG_INFO("CustomComponents/Pick3/META_ISO: mode=%d (0=normal 1=no-rim 2=no-stock-icons 3=both-off 4=no-metadata)",mode);
        return mode;
    }
    inline void Trace(const char* phase,int option,int guid) {
        LOG_INFO("CustomComponents/Pick3/META_ISO: phase=%s option=%d guid=%d mode=%d",phase,option+1,guid,mode);
    }
}
