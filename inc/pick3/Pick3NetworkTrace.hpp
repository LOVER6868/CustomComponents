#pragma once
// Read-only, ADB-switchable Pick3/PvP pipeline diagnostics.
// Intentionally never constructs, mutates, resends or completes a Play.
#include <atomic>
#include <array>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <mutex>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "log.h"

namespace CustomComponents::Pick3::NetworkTrace {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
// Keep consistent with existing ADB diagnostic file convention.
inline constexpr const char* kSwitch = "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt";
inline std::atomic<int> mode{0};
inline std::atomic<std::time_t> lastSwitchRead{0};
inline std::atomic<unsigned long long> nextId{1};
inline bool installed = false;
inline BNM::Field<int> typeField, playerField, cardField, orderField;
inline BNM::Field<IntList*> targetsField;
inline BNM::Field<int> effectEntityField, effectGroupField;
inline Obj* (*originalSerialize)(Obj*, Obj*, const void*) = nullptr;
inline void (*originalRemoteProcess)(Obj*, Obj*, const void*) = nullptr;
inline Obj* (*originalDecide)(Obj*, Obj*, const void*) = nullptr;
inline void (*originalClientSimulate)(Obj*, Obj*, Obj*, const void*) = nullptr;
inline void (*originalServerSimulate)(Obj*, Obj*, Obj*, const void*) = nullptr;
inline void (*originalClientSuccess)(Obj*, const void*) = nullptr;
inline void (*originalClientFailure)(Obj*, int, const void*) = nullptr;
inline void (*originalPredictionSuccess)(Obj*, const void*) = nullptr;
inline void (*originalPredictionFailure)(Obj*, int, const void*) = nullptr;
inline void (*originalServerSuccess)(Obj*, const void*) = nullptr;
inline void (*originalServerFailure)(Obj*, const void*) = nullptr;
inline void (*originalLocalSuccess)(Obj*, const void*) = nullptr;
inline void (*originalLocalFailure)(Obj*, int, const void*) = nullptr;
inline Obj* (*originalQueueShift)(Obj*, const void*) = nullptr;
// Connector Send returns a 4-byte PvpPlayRequestId value (UniqueId in dump.cs).
inline int (*originalConnectorSend)(Obj*, Obj*, Obj*, const void*) = nullptr;
inline int (*originalConnectorSendHandlers)(Obj*, Obj*, Obj*, Obj*, Obj*, const void*) = nullptr;
inline void (*originalPostSuccess)(Obj*, const void*) = nullptr;
inline BNM::Field<int> postRequestIdField;
inline Obj* (*originalQueueDoShift)(Obj*, const void*) = nullptr;
inline void (*originalCompleteEffect)(Obj*, Obj*, const void*) = nullptr;
inline void (*originalProcessEffects)(Obj*, Obj*, const void*) = nullptr;
inline BNM::Class queueClass;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> queueEffects, queueResponses, queueGroupResponses;
inline BNM::Field<int> queueCurrentGroup;
// Scoped to the game callback currently executing. This is not a global
// 'last request' guess: overlapping multiplayer threads cannot borrow it.


struct Flight {
    Obj* play = nullptr;        // observational identity only; NEVER dereference after callback
    Obj* simulator = nullptr;   // observational identity only
    unsigned long long id = 0;
    int effect = -1, group = -1;
    int type = -1;
    bool pick3 = false;
    int requestId = -1;
};
inline std::mutex flightsMutex;
inline std::array<Flight, 48> flights{};
inline std::size_t nextSlot = 0;
inline unsigned long long currentPick3 = 0;
inline int currentEffect = -1, currentGroup = -1;
inline thread_local Flight callbackFlight{};


inline int Mode() {
    const auto now = std::time(nullptr);
    auto seen = lastSwitchRead.load(std::memory_order_relaxed);
    if (now != seen && lastSwitchRead.compare_exchange_strong(seen, now)) {
        int next = 0;
        if (FILE* file = std::fopen(kSwitch, "rb")) {
            int parsed = 0;
            if (std::fscanf(file, "%d", &parsed) == 1 && parsed >= 0 && parsed <= 3) next = parsed;
            std::fclose(file);
        }
        const int old = mode.exchange(next);
        if (next != old) LOG_INFO("CustomComponents/Pick3/NETTRACE: SWITCH mode=%d previous=%d", next, old);
    }
    return mode.load(std::memory_order_relaxed);
}
inline bool MetaReady() {
    return typeField.IsValid() && playerField.IsValid() && cardField.IsValid() &&
        orderField.IsValid() && targetsField.IsValid();
}
inline bool CapturePlay(Obj* play, int& type, int& player, int& card, int& order, int& count,
                        int& t0, int& t1, int& t2) {
    if (!play || !MetaReady()) return false;
    type = typeField[play].Get();
    player = playerField[play].Get();
    card = cardField[play].Get();
    order = orderField[play].Get();
    count = 0; t0 = t1 = t2 = -1;
    IntList* list = targetsField[play].Get();
    if (!list) return true;
    // Read at most three entries; do not allocate/copy large lists in a network hook.
    count = list->size;
    if (count < 0 || count > 256 || (count > 0 && !list->items)) return false;
    if (count > 0) t0 = list->items->m_Items[0];
    if (count > 1) t1 = list->items->m_Items[1];
    if (count > 2) t2 = list->items->m_Items[2];
    return true;
}
inline Flight FindFlight(Obj* play, Obj* simulator = nullptr) {
    std::lock_guard<std::mutex> lock(flightsMutex);
    // Prefer an explicit simulator association for asynchronous callbacks.
    if (simulator) for (const auto& f : flights) if (f.simulator == simulator && f.id) return f;
    if (play) for (const auto& f : flights) if (f.play == play && f.id) return f;
    return {};
}
struct ScopedCallbackFlight {
    Flight previous;
    explicit ScopedCallbackFlight(Obj* simulator) : previous(callbackFlight) {
        callbackFlight=FindFlight(nullptr, simulator);
    }
    ~ScopedCallbackFlight() { callbackFlight=previous; }
};
inline void Track(Obj* play, Obj* sim, Flight f) {
    std::lock_guard<std::mutex> lock(flightsMutex);
    if (sim) for (auto& old : flights) {
        if (old.simulator == sim && old.id) {
            old = f; old.play = play; old.simulator = sim; return;
        }
    }
    if (play) for (auto& old : flights) {
        if (old.play == play && old.id) {
            old = f; old.play = play; if (sim) old.simulator = sim; return;
        }
    }
    auto& target = flights[nextSlot++ % flights.size()];
    target = f; target.play = play; target.simulator = sim;
}
inline void AssociateRequest(int request) {
    if (!callbackFlight.id || request < 0) return;
    std::lock_guard<std::mutex> lock(flightsMutex);
    for(auto& f:flights) if (f.id==callbackFlight.id) {f.requestId=request;break;}
}
inline Flight FindRequest(int request) {
    if (request < 0) return {};
    std::lock_guard<std::mutex> lock(flightsMutex);
    for(const auto& f:flights) if(f.id && f.requestId==request) return f;
    return {};
}
inline bool ShouldLog(Obj* play, Obj* sim = nullptr) {
    const int m = Mode();
    if (!m) return false;
    if (m == 3) return true;
    auto f = FindFlight(play, sim);
    if (m == 1) return f.pick3;
    if (f.pick3) return true;
    if (play && typeField.IsValid()) return typeField[play].Get() == 6;
    return f.type == 6;
}
inline unsigned long long IdFor(Obj* play, Obj* sim = nullptr) {
    auto f = FindFlight(play, sim);
    return f.id;
}
inline const char* ClassName(Obj* o) {
    return o && o->klass && o->klass->name ? o->klass->name : "null";
}
inline void PlayStage(const char* stage, Obj* play, Obj* sim = nullptr) {
    if (!ShouldLog(play, sim)) return;
    int type=-1, player=-1, card=-1, order=-1, count=-1, a=-1, b=-1, c=-1;
    if (!CapturePlay(play, type, player, card, order, count, a, b, c)) {
        auto f=FindFlight(play,sim);
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=%s id=%llu play=%p sim=%p type=%d effect=%d group=%d payloadUnavailable=1",
            stage, f.id, static_cast<void*>(play), static_cast<void*>(sim),f.type,f.effect,f.group);
        return;
    }
    auto f=FindFlight(play,sim);
    LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=%s id=%llu pick3=%d effect=%d group=%d play=%p sim=%p type=%d player=%d card=%d order=%d targets=%d [%d,%d,%d]",
        stage,f.id,f.pick3?1:0,f.effect,f.group,static_cast<void*>(play),static_cast<void*>(sim),
        type,player,card,order,count,a,b,c);
}
inline void Callback(const char* name, Obj* sim, int failure = -1) {
    if (!ShouldLog(nullptr, sim)) return;
    const auto f=FindFlight(nullptr, sim);
    LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=%s id=%llu pick3=%d effect=%d group=%d sim=%p resultCode=%d",
       name,f.id,f.pick3?1:0,f.effect,f.group,static_cast<void*>(sim),failure);
}
inline void MarkPrepared(Obj* play, int effect, int group) {
    if (!Mode() || !play) return;
    Flight f{}; f.id=nextId.fetch_add(1); f.effect=effect; f.group=group; f.pick3=true; f.type=6;
    Track(play,nullptr,f);
    { std::lock_guard<std::mutex> lock(flightsMutex); currentPick3=f.id; currentEffect=effect; currentGroup=group; }
    PlayStage("PICK3_PREPARED", play);
}
inline void MarkStage(const char* stage, Obj* play) { PlayStage(stage,play); }
inline void MarkResult(const char* stage, int effect, int group, int code) {
    if (!Mode()) return;
    unsigned long long id=0;
    { std::lock_guard<std::mutex> lock(flightsMutex);
      for (const auto& f:flights) if (f.pick3 && f.effect==effect && f.group==group) {id=f.id;break;} }
    LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=%s id=%llu effect=%d group=%d resultCode=%d",
             stage,id,effect,group,code);
}
inline void OnServiceResult(bool success, int failure) {
    if (!Mode()) return;
    unsigned long long id=0; int effect=-1, group=-1;
    { std::lock_guard<std::mutex> lock(flightsMutex);id=currentPick3;effect=currentEffect;group=currentGroup; }
    LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=PICK3_SERVICE_CALLBACK id=%llu effect=%d group=%d success=%d reason=%d (may be local prediction)",
        id,effect,group,success?1:0,failure);
}
// Existing input observer samples once per frame. Do not detour the tiny
// VisualTurnPhaseModel.StartSubTurn/EndSubTurn/field-setter methods.
inline void ObserveVisualSubturn(Obj* phase, int subturn, bool customCapture) {
    static Obj* lastPhase = nullptr;
    static int lastSubturn = -999;
    const int m=Mode();
    if (!m) { lastPhase=nullptr; lastSubturn=-999; return; }
    if (phase==lastPhase && subturn==lastSubturn) return;
    const int previous=lastSubturn;
    lastPhase=phase; lastSubturn=subturn;
    if (m>=2 || customCapture) {
        Flight f{};
        { std::lock_guard<std::mutex> lock(flightsMutex);
          for (const auto& candidate:flights)
            if (candidate.pick3 && candidate.id > f.id) f=candidate; }
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=VISUAL_SUBTURN_CHANGED id=%llu phase=%p previous=%d current=%d capture=%d lastEffect=%d lastGroup=%d (observation only)",
            f.id,static_cast<void*>(phase),previous,subturn,customCapture?1:0,f.effect,f.group);
    }
}
inline void ResetMatch() {
    std::lock_guard<std::mutex> lock(flightsMutex);
    flights={}; nextSlot=0; currentPick3=0; currentEffect=currentGroup=-1;
}

static void RemoteProcess(Obj* self, Obj* play, const void* method) {
    PlayStage("REMOTE_PROCESS_ENTER",play);
    if (originalRemoteProcess) originalRemoteProcess(self,play,method);
    PlayStage("REMOTE_PROCESS_RETURN",play);
}
static Obj* Decide(Obj* self, Obj* play, const void* method) {
    PlayStage("DECIDER_ENTER",play);
    Obj* result = originalDecide ? originalDecide(self,play,method) : nullptr;
    if (ShouldLog(play))
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=DECIDER_RETURN id=%llu simulator=%p class=%s",
            IdFor(play),static_cast<void*>(result),ClassName(result));
    return result;
}
static Obj* Serialize(Obj* self, Obj* play, const void* method) {
    PlayStage("SERIALIZE_ENTER",play);
    Obj* result=originalSerialize ? originalSerialize(self,play,method) : nullptr;
    if (ShouldLog(play))
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=SERIALIZE_RETURN id=%llu bytesObject=%p nonnull=%d (not network ACK)",
                IdFor(play),static_cast<void*>(result),result?1:0);
    return result;
}
static void ClientSimulate(Obj* self, Obj* play, Obj* service, const void* method) {
    if (Mode() && play && !IdFor(play)) {
        int type=typeField.IsValid()?typeField[play].Get():-1;
        if (Mode()>=2 && (type==6 || Mode()==3)) { Flight f{}; f.id=nextId.fetch_add(1);f.type=type; Track(play,self,f); }
    }
    auto f=FindFlight(play); if (f.id) Track(play,self,f);
    PlayStage("CLIENT_SIMULATE_ENTER",play,self);
    if (originalClientSimulate) originalClientSimulate(self,play,service,method);
    PlayStage("CLIENT_SIMULATE_RETURN",play,self);
}
static void ServerSimulate(Obj* self, Obj* play, Obj* service, const void* method) {
    if (Mode() && play && !IdFor(play)) {
        int type=typeField.IsValid()?typeField[play].Get():-1;
        if (Mode()>=2 && (type==6 || Mode()==3)) { Flight f{}; f.id=nextId.fetch_add(1);f.type=type; Track(play,self,f); }
    }
    auto f=FindFlight(play); if (f.id) Track(play,self,f);
    PlayStage("SERVER_SIMULATE_ENTER",play,self);
    if (originalServerSimulate) originalServerSimulate(self,play,service,method);
    PlayStage("SERVER_SIMULATE_RETURN",play,self);
}
static void ClientSuccess(Obj* self, const void* method) {
    Callback("CLIENT_SERVICE_SUCCESS_ENTER",self);
    ScopedCallbackFlight correlation(self);
    if (originalClientSuccess) originalClientSuccess(self,method);
    Callback("CLIENT_SERVICE_SUCCESS_RETURN",self);
}
static void ClientFailure(Obj* self, int reason, const void* method) {
    Callback("CLIENT_SERVICE_FAILURE_ENTER",self,reason);
    if (originalClientFailure) originalClientFailure(self,reason,method);
    Callback("CLIENT_SERVICE_FAILURE_RETURN",self,reason);
}
static void PredictionSuccess(Obj* self, const void* method) {
    Callback("PREDICTION_SUCCESS_ENTER",self);
    ScopedCallbackFlight correlation(self);
    if (originalPredictionSuccess) originalPredictionSuccess(self,method);
    Callback("PREDICTION_SUCCESS_RETURN",self);
}
static void PredictionFailure(Obj* self, int reason, const void* method) {
    Callback("PREDICTION_FAILURE_ENTER",self,reason);
    if (originalPredictionFailure) originalPredictionFailure(self,reason,method);
    Callback("PREDICTION_FAILURE_RETURN",self,reason);
}
static void ServerSuccess(Obj* self, const void* method) {
    Callback("SERVER_SUCCESS_ENTER",self);
    ScopedCallbackFlight correlation(self);
    if (originalServerSuccess) originalServerSuccess(self,method);
    Callback("SERVER_SUCCESS_RETURN",self);
}
static void ServerFailure(Obj* self, const void* method) {
    Callback("SERVER_FAILURE_ENTER",self);
    if (originalServerFailure) originalServerFailure(self,method);
    Callback("SERVER_FAILURE_RETURN",self);
}
static void LocalSuccess(Obj* self, const void* method) {
    Callback("LOCAL_SERVICE_SUCCESS_ENTER",self);
    ScopedCallbackFlight correlation(self);
    if (originalLocalSuccess) originalLocalSuccess(self,method);
    Callback("LOCAL_SERVICE_SUCCESS_RETURN",self);
}
static void LocalFailure(Obj* self, int reason, const void* method) {
    Callback("LOCAL_SERVICE_FAILURE_ENTER",self,reason);
    if (originalLocalFailure) originalLocalFailure(self,reason,method);
    Callback("LOCAL_SERVICE_FAILURE_RETURN",self,reason);
}
// PvPGameConnectorImpl.Send(...): read-only interception of the existing send.
// Does not log packet contents, patch callbacks, or create extra requests.
static int ConnectorSend(Obj* self, Obj* type, Obj* bytes, const void* method) {
    const auto f=callbackFlight;
    const int m=Mode();
    const bool log=(m==3 || (m>=2 && f.id) || (m==1 && f.pick3));
    const auto data=reinterpret_cast<BNM::Structures::Mono::Array<unsigned char>*>(bytes);
    const unsigned long long length=data ? static_cast<unsigned long long>(data->capacity) : 0;
    if (log) LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=CONNECTOR_SEND_ENTER id=%llu pick3=%d effect=%d group=%d connector=%p data=%p byteCount=%llu hasHandlers=0",
        f.id,f.pick3?1:0,f.effect,f.group,static_cast<void*>(self),static_cast<void*>(bytes),length);
    int request=originalConnectorSend ? originalConnectorSend(self,type,bytes,method) : -1;
    AssociateRequest(request);
    if (log) LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=CONNECTOR_SEND_RETURN id=%llu requestId=%d (queued, not acknowledged)",f.id,request);
    return request;
}
static int ConnectorSendHandlers(Obj* self, Obj* type, Obj* bytes, Obj* onOk, Obj* onFail, const void* method) {
    const auto f=callbackFlight;
    const int m=Mode();
    const bool log=(m==3 || (m>=2 && f.id) || (m==1 && f.pick3));
    const auto data=reinterpret_cast<BNM::Structures::Mono::Array<unsigned char>*>(bytes);
    const unsigned long long length=data ? static_cast<unsigned long long>(data->capacity) : 0;
    if (log) LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=CONNECTOR_POST_WITH_HANDLERS_ENTER id=%llu pick3=%d effect=%d group=%d connector=%p data=%p byteCount=%llu hasHandlers=1",
        f.id,f.pick3?1:0,f.effect,f.group,static_cast<void*>(self),static_cast<void*>(bytes),length);
    int request=originalConnectorSendHandlers ? originalConnectorSendHandlers(self,type,bytes,onOk,onFail,method) : -1;
    AssociateRequest(request);
    if (log) LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=CONNECTOR_POST_WITH_HANDLERS_RETURN id=%llu requestId=%d (posted, not game ACK)",f.id,request);
    return request;
}
// HTTP-post success means the request completed at the transport layer; it
// does not imply that the remote simulation evaluated the custom effect.
static void PosterRequestSuccess(Obj* self, const void* method) {
    const int m=Mode();
    int request = self && postRequestIdField.IsValid() ? postRequestIdField[self].Get() : -1;
    Flight f = FindRequest(request);
    if ((m==3 || (m==2 && f.id) || (m==1 && f.pick3)))
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=POST_REQUEST_SUCCESS_ENTER id=%llu requestId=%d effect=%d group=%d (transport callback, NOT gameplay approval)",
            f.id,request,f.effect,f.group);
    if (originalPostSuccess) originalPostSuccess(self, method);
    if ((m==3 || (m==2 && f.id) || (m==1 && f.pick3)))
        LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=POST_REQUEST_SUCCESS_RETURN id=%llu requestId=%d",f.id,request);
}
// Passive inspection of the effect collection held by the game engine. These
// methods only inspect List<Effect> headers and IDs of still-live entries.
struct QueueView {
    bool valid=false;
    int currentGroup=-1, effects=-1, responses=-1, groupResponses=-1;
    int firstEntity=-1, firstGroup=-1;
    unsigned long long matchingId=0;
    int matchingIndex=-1, matchingList=-1, matchingEffect=-1, matchingGroup=-1;
};
inline int QueueCount(BNM::Structures::Mono::List<Obj*>* list) {
    if (!list) return -1;
    return list->size >= 0 && list->size <= 20000 ? list->size : -1;
}
inline QueueView ReadQueue(Obj* q) {
    QueueView v{};
    if (!q || !queueClass.IsValid() || !BNM::IsA(q,queueClass) || !queueEffects.IsValid() ||
         !effectEntityField.IsValid() || !effectGroupField.IsValid()) return v;
    v.valid=true;
    v.currentGroup=queueCurrentGroup.IsValid()?queueCurrentGroup[q].Get():-1;
    auto* entries=queueEffects[q].Get();
    auto* responses=queueResponses.IsValid()?queueResponses[q].Get():nullptr;
    auto* groupResponses=queueGroupResponses.IsValid()?queueGroupResponses[q].Get():nullptr;
    v.effects=QueueCount(entries);
    v.responses=QueueCount(responses);
    v.groupResponses=QueueCount(groupResponses);
    std::array<Flight,48> snapshot;
    {std::lock_guard<std::mutex> lock(flightsMutex);snapshot=flights;}
    // A blocking effect can move into a response list before retirement.
    // Inspect all three live lists; never call Resolve(), Shift(), or Peek().
    const auto scan=[&](BNM::Structures::Mono::List<Obj*>* list,int count,int listId) {
        if (count<=0 || !list || !list->items || v.matchingId) return;
        for(int i=0;i<count && i<128;i++) {
            Obj* e=list->items->m_Items[i];
            if (!e) continue;
            const int entity=effectEntityField[e].Get();
            const int group=effectGroupField[e].Get();
            if (listId==0 && i==0) {v.firstEntity=entity;v.firstGroup=group;}
            for(const auto& f:snapshot) if(f.pick3 && f.id && f.effect==entity && f.group==group) {
                v.matchingId=f.id;v.matchingList=listId;v.matchingEffect=entity;
                v.matchingGroup=group;v.matchingIndex=i;break;
            }
            if(v.matchingId) break;
        }
    };
    scan(entries,v.effects,0);
    scan(responses,v.responses,1);
    scan(groupResponses,v.groupResponses,2);
    return v;
}

inline void QueueStage(const char* stage, Obj* q, QueueView v, QueueView before = {}) {
    const int m=Mode();
    if (!m) return;
    if (m<3 && !v.matchingId && !before.matchingId) return;
    LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=%s id=%llu queue=%p group=%d active=%d responses=%d groupResponses=%d front=%d/%d matchList=%d matchIndex=%d previouslyActive=%d previousMatchList=%d previousMatchIndex=%d",
        stage, v.matchingId?v.matchingId:before.matchingId,static_cast<void*>(q),v.currentGroup,
        v.effects,v.responses,v.groupResponses,v.firstEntity,v.firstGroup,
        v.matchingList,v.matchingIndex,before.effects,before.matchingList,before.matchingIndex);
}
static Obj* QueueDoShift(Obj* self, const void* method) {
    // Queue snapshots are taken before and after original, never modified.
    QueueView before{};
    if (Mode()) before=ReadQueue(self);
    Obj* result=originalQueueDoShift ? originalQueueDoShift(self,method):nullptr;
    if (Mode()) {
        const auto after=ReadQueue(self);
        QueueStage("QUEUE_DOSHIFT_RETURN",self,after,before);
        // The returned Effect is expected to remain alive until caller consumes it.
        // Avoid dereferencing here because IL2CPP may already pool retired items.
    }
    return result;
}
static void CompleteEffect(Obj* self, Obj* queue, const void* method) {
    QueueView before{};
    if (Mode()) before=ReadQueue(queue);
    if (originalCompleteEffect) originalCompleteEffect(self,queue,method);
    if (Mode()) QueueStage("COMPLETE_EFFECT_RETURN",queue,ReadQueue(queue),before);
}
static void ProcessEffects(Obj* self, Obj* queue, const void* method) {
    QueueView before{};
    if (Mode()) before=ReadQueue(queue);
    if (originalProcessEffects) originalProcessEffects(self,queue,method);
    if (Mode()) QueueStage("PROCESS_EFFECTS_RETURN",queue,ReadQueue(queue),before);
}
static Obj* QueueShift(Obj* self, const void* method) {
    Obj* shifted=originalQueueShift ? originalQueueShift(self,method) : nullptr;
    if (!Mode() || !shifted || !effectEntityField.IsValid() || !effectGroupField.IsValid()) return shifted;
    const int e=effectEntityField[shifted].Get(), g=effectGroupField[shifted].Get();
    std::array<Flight,48> snapshot;
    { std::lock_guard<std::mutex> lock(flightsMutex); snapshot=flights; }
    for (const auto& f:snapshot) {
        if (f.pick3 && f.effect==e && f.group==g) {
            LOG_INFO("CustomComponents/Pick3/NETTRACE: stage=EFFECT_QUEUE_SHIFT id=%llu effect=%d group=%d queue=%p",
                f.id,e,g,static_cast<void*>(self));break;
        }
    }
    return shifted;
}
inline void Initialize() {
    if (installed) return;
    installed=true;
    auto play=BNM::Class("PvZCards.Engine.Dtos","Play");
    typeField=play.GetField("Type").cast<int>(); playerField=play.GetField("PlayerId").cast<int>();
    cardField=play.GetField("CardId").cast<int>(); orderField=play.GetField("OrderInTargetLocation").cast<int>();
    targetsField=play.GetField("Targets").cast<IntList*>();
    auto ns=BNM::Class("PvZCards.Multiplayer.Networking","RemotePlayService");
    auto decider=BNM::Class("PvZCards.Multiplayer.Networking","PlaySimulatorDecider");
    auto serializer=BNM::Class("PvZCards.Multiplayer.Networking","GameCommandSerializer");
    auto client=BNM::Class("PvZCards.Multiplayer.Networking","ClientAuthoritativePlaySimulator");
    auto server=BNM::Class("PvZCards.Multiplayer.Networking","ServerAuthoritativePlaySimulator");
    auto effect=BNM::Class("PvZCards.Engine","Effect");
    effectEntityField=effect.GetField("EffectEntityId").cast<int>();
    effectGroupField=effect.GetField("EffectGroupId").cast<int>();
    queueClass=BNM::Class("PvZCards.Engine","EffectQueue");
    queueEffects=queueClass.GetField("Effects").cast<BNM::Structures::Mono::List<Obj*>*>();
    queueResponses=queueClass.GetField("ResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
    queueGroupResponses=queueClass.GetField("GroupResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
    queueCurrentGroup=queueClass.GetField("CurrentGroupId").cast<int>();
    auto doShift=queueClass.GetMethod("DoShift",0);
    auto processor=BNM::Class("PvZCards.Engine","EffectCollectionProcessor");
    auto complete=processor.GetMethod("CompleteEffect",1);
    auto processEffects=processor.GetMethod("ProcessEffects",1);
    auto connector=BNM::Class("PvZCards.Multiplayer.Networking","PvPGameConnectorImpl");
    auto connectorSend=connector.GetMethod("Send",2);
    auto postWithHandlers=connector.GetMethod("PostWithHandlers",4);
    auto poster=BNM::Class("PvZCards.Multiplayer.Networking","PvpPlayPoster");
    auto posterRequest=poster.GetInnerClass("PvpPlayRequest");
    auto postSuccess=posterRequest.GetMethod("CompleteWithSuccess",0);
    postRequestIdField=posterRequest.GetField("Id").cast<int>();
    auto shift=BNM::Class("PvZCards.Engine","EffectQueue").GetMethod("Shift",0);
    auto a=ns.GetMethod("Process",1), b=decider.GetMethod("GetPlaySimulator",1);
    auto c=serializer.GetMethod("SerializePlay",1);
    auto cs=client.GetMethod("Simulate",2), ss=server.GetMethod("Simulate",2);
    auto cOk=client.GetMethod("HandleServiceSuccess",0),cFail=client.GetMethod("HandleServiceFailure",1);
    auto pOk=server.GetMethod("HandlePredictionSuccess",0),pFail=server.GetMethod("HandlePredictionFailure",1);
    auto sOk=server.GetMethod("HandleServerSuccess",0),sFail=server.GetMethod("HandleServerFailure",0);
    auto lOk=server.GetMethod("HandleLocalServiceSuccess",0),lFail=server.GetMethod("HandleLocalServiceFailure",1);
    if(a.IsValid()) BNM::BasicHook(a,RemoteProcess,originalRemoteProcess);
    if(b.IsValid()) BNM::BasicHook(b,Decide,originalDecide);
    if(c.IsValid()) BNM::BasicHook(c,Serialize,originalSerialize);
    if(cs.IsValid()) BNM::BasicHook(cs,ClientSimulate,originalClientSimulate);
    if(ss.IsValid()) BNM::BasicHook(ss,ServerSimulate,originalServerSimulate);
    if(cOk.IsValid()) BNM::BasicHook(cOk,ClientSuccess,originalClientSuccess);
    if(cFail.IsValid()) BNM::BasicHook(cFail,ClientFailure,originalClientFailure);
    if(pOk.IsValid()) BNM::BasicHook(pOk,PredictionSuccess,originalPredictionSuccess);
    if(pFail.IsValid()) BNM::BasicHook(pFail,PredictionFailure,originalPredictionFailure);
    if(sOk.IsValid()) BNM::BasicHook(sOk,ServerSuccess,originalServerSuccess);
    if(sFail.IsValid()) BNM::BasicHook(sFail,ServerFailure,originalServerFailure);
    if(lOk.IsValid()) BNM::BasicHook(lOk,LocalSuccess,originalLocalSuccess);
    if(lFail.IsValid()) BNM::BasicHook(lFail,LocalFailure,originalLocalFailure);
    if(shift.IsValid()) BNM::BasicHook(shift,QueueShift,originalQueueShift);
    if(doShift.IsValid()) BNM::BasicHook(doShift,QueueDoShift,originalQueueDoShift);
    if(complete.IsValid()) BNM::BasicHook(complete,CompleteEffect,originalCompleteEffect);
    if(processEffects.IsValid()) BNM::BasicHook(processEffects,ProcessEffects,originalProcessEffects);
    if(connectorSend.IsValid()) BNM::BasicHook(connectorSend,ConnectorSend,originalConnectorSend);
    if(postWithHandlers.IsValid()) BNM::BasicHook(postWithHandlers,ConnectorSendHandlers,originalConnectorSendHandlers);
    if(postSuccess.IsValid() && postRequestIdField.IsValid()) BNM::BasicHook(postSuccess,PosterRequestSuccess,originalPostSuccess);
    // The four-argument Send overload itself is an 8-byte forwarding stub.
    // Don't detour it: PostWithHandlers is the safe full implementation.

    LOG_INFO("CustomComponents/Pick3/NETTRACE: hooks remote=%d decider=%d serializer=%d clientSim=%d serverSim=%d clientOK=%d clientFail=%d predictionOK=%d predictionFail=%d serverOK=%d serverFail=%d localOK=%d localFail=%d queueShift=%d doShift=%d complete=%d process=%d connector=%d postHandlers=%d requestOK=%d meta=%d switch=%s (default OFF)",
        originalRemoteProcess!=nullptr,originalDecide!=nullptr,originalSerialize!=nullptr,
        originalClientSimulate!=nullptr,originalServerSimulate!=nullptr,
        originalClientSuccess!=nullptr,originalClientFailure!=nullptr,
        originalPredictionSuccess!=nullptr,originalPredictionFailure!=nullptr,
        originalServerSuccess!=nullptr,originalServerFailure!=nullptr,
        originalLocalSuccess!=nullptr,originalLocalFailure!=nullptr,originalQueueShift!=nullptr,
        originalQueueDoShift!=nullptr,originalCompleteEffect!=nullptr,originalProcessEffects!=nullptr,
        originalConnectorSend!=nullptr,originalConnectorSendHandlers!=nullptr,
        originalPostSuccess!=nullptr,MetaReady()?1:0,kSwitch);
}
} // namespace CustomComponents::Pick3::NetworkTrace
