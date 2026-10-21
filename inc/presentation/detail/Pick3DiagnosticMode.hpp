#pragma once
// Device-side diagnostic control. The game owns its Android/data directory.
// Mode changes apply to the NEXT popup and do not require reinstalling the APK.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "log.h"
namespace CustomComponents::Presentation::Pick3DiagnosticMode {
    inline constexpr const char* kDirectory="/sdcard/Android/data/com.ea.gp.pvzheroes/files";
    inline constexpr const char* kModeFile="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_diag_mode.txt";
    inline constexpr const char* kMarkerFile="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_diag_marker.txt";
    inline bool enabled=true;
    inline int mode=0; // 0=static, 1=metadata, 2=one native, 3=one RT, 4=all RTs
    inline bool recovered=false;
    inline bool startupChecked=false;
    inline int announcedMode=-1;
    inline void Mark(const char* phase,int option,int guid) {
        if (!enabled) return;
        if (FILE* f=std::fopen(kMarkerFile,"wb")) {
            std::fprintf(f,"PENDING phase=%s option=%d guid=%d mode=%d\n",phase,option,guid,mode);
            std::fflush(f); ::fsync(::fileno(f)); std::fclose(f);
        }
        LOG_INFO("CustomComponents/Pick3/DIAG: ENTER phase=%s option=%d guid=%d mode=%d",phase,option,guid,mode);
    }
    inline void Complete(const char* phase,int option,int guid) {
        if (!enabled) return;
        if (FILE* f=std::fopen(kMarkerFile,"wb")) {
            std::fprintf(f,"OK phase=%s option=%d guid=%d mode=%d\n",phase,option,guid,mode);
            std::fflush(f);::fsync(::fileno(f));std::fclose(f);
        }
        LOG_INFO("CustomComponents/Pick3/DIAG: EXIT phase=%s option=%d guid=%d mode=%d",phase,option,guid,mode);
    }
    inline void Startup() {
        if (startupChecked) return;
        startupChecked=true;
        char state[256]{};
        if(FILE* f=std::fopen(kMarkerFile,"rb")) {
            std::fgets(state,sizeof(state),f);std::fclose(f);
        }
        if(std::strncmp(state,"PENDING ",8)==0) {
            recovered=true;
            LOG_ERR("CustomComponents/Pick3/DIAG: RECOVERY previous unfinished operation: %s",state);
        }
    }
    inline int ReadMode() {
        Startup();
        int requested=0;
        if(FILE* f=std::fopen(kModeFile,"rb")) {
            if(std::fscanf(f,"%d",&requested)!=1)requested=0;
            std::fclose(f);
        }
        requested=std::clamp(requested,0,4);
        // A crash inside a marked native operation disables native mode until
        // the user explicitly deletes the marker. Do not automatically retry.
        mode=recovered?0:requested;
        if(mode!=announcedMode){
            announcedMode=mode;
            LOG_INFO("CustomComponents/Pick3/DIAG: MODE=%d requested=%d recovered=%d path=%s",mode,requested,recovered?1:0,kModeFile);
        }
        return mode;
    }
    inline void ResetRecovery() {
        recovered=false;
        Complete("manual-reset",0,0);
    }
}
