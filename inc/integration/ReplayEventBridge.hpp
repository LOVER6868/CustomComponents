#pragma once
// PvZH replay event journal ABI 1: optional, ID-only, producer-owned.
// CustomComponents may publish with no MatchRecorder installed. MatchRecorder
// may include this header with no CustomComponents module installed.
// There are no managed pointers, callbacks, game hooks, file operations or
// allocations in Publish(). A slow/missing consumer never blocks gameplay
// beyond taking a short native mutex; old events are overwritten and counted.
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <vector>
namespace CustomComponents::Integration::ReplayEventBridge {
inline constexpr std::uint32_t kSchemaVersion = 1;
inline constexpr std::size_t kCapacity = 1024;
inline constexpr std::size_t kMaxFields = 10;
struct Value { const char* key; std::int64_t number; };
struct Field { char key[28]{}; std::int64_t number=0; };
struct Event {
    std::uint64_t serial=0;
    char name[52]{};
    std::uint8_t fieldCount=0;
    std::array<Field,kMaxFields> fields{};
};
struct Batch {
    std::uint64_t lastSerial=0;
    std::uint64_t lostBeforeRead=0;
    std::vector<Event> events;
};
inline std::mutex journalMutex;
inline std::array<Event,kCapacity> journal{};
inline std::uint64_t lastSerial=0;
inline std::size_t count=0;
inline std::uint64_t totalOverwritten=0;
inline bool enabled=true;
inline void CopyLabel(char* dest,std::size_t cap,const char* src) {
    if(!dest || !cap)return;
    std::size_t n=0;
    if(src)while(src[n] && n+1<cap){
        const char ch=src[n];
        // Event/key labels are wire-format identifiers, not free-form text.
        dest[n]=(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
                (ch>='0'&&ch<='9')||ch=='_'?ch:'_';
        ++n;
    }
    dest[n]='\0';
}
inline void Publish(const char* name,std::initializer_list<Value> values={}) {
    if(!enabled || !name || !*name)return;
    Event event{};
    CopyLabel(event.name,sizeof(event.name),name);
    for(const auto& entry:values) {
        if(event.fieldCount>=kMaxFields)break;
        auto& slot=event.fields[event.fieldCount++];
        CopyLabel(slot.key,sizeof(slot.key),entry.key);
        slot.number=entry.number;
    }
    std::lock_guard<std::mutex> lock(journalMutex);
    event.serial=++lastSerial;
    journal[(lastSerial-1)%kCapacity]=event;
    if(count<kCapacity)++count;
    else ++totalOverwritten;
}
inline std::uint64_t CurrentSerial() {
    std::lock_guard<std::mutex> lock(journalMutex);
    return lastSerial;
}
inline Batch ReadAfter(std::uint64_t cursor) {
    Batch batch{};
    std::lock_guard<std::mutex> lock(journalMutex);
    batch.lastSerial=lastSerial;
    const auto earliest=count?lastSerial-count+1:lastSerial+1;
    if(cursor+1<earliest)batch.lostBeforeRead=earliest-(cursor+1);
    const auto first=cursor+1>earliest?cursor+1:earliest;
    if(first<=lastSerial){
        batch.events.reserve(static_cast<std::size_t>(lastSerial-first+1));
        for(auto serial=first;serial<=lastSerial;++serial)
            batch.events.push_back(journal[(serial-1)%kCapacity]);
    }
    return batch;
}
} // namespace CustomComponents::Integration::ReplayEventBridge
