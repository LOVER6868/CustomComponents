#pragma once
extern "C" void MatchRecorder_ObservePresentation(const char*,void*,void*,int,int) __attribute__((weak));
namespace CustomComponents::Core {
inline void ObserveReplayPresentation(const char* tag,void* self,void* subject,int group=-1,int card=-1){
    if(MatchRecorder_ObservePresentation)MatchRecorder_ObservePresentation(tag,self,subject,group,card);
}
}
