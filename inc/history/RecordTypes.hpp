#pragma once
#include <cstdint>
#include <variant>
#include <string>
#include <vector>
#include <optional>
namespace CustomComponents::History {
inline constexpr unsigned SchemaVersion=2;
enum class Kind:std::uint16_t {DiscardOutsidePlay=1,DeckReorder,ReturnFromDiscard,ZoneTransfer,Selection,StateChange,CardCreated,Presentation,SwapTransaction,VariableChanged,LevelChanged,SelectionRequest,SelectionResolved,CardMetadataChanged,CombatEncounter,AbilityChanged,DeathCancelled,DamageRule,HealingRule,PresentationEvent};
enum class Origin:std::uint8_t {Unknown,Hand,Deck,DiscardPile,Lane,Pool};
enum class Stage:std::uint8_t {Requested,Observed,Committed,Cancelled,PartialFailure};
enum class Delivery:std::uint8_t {Pending,Processing,Processed,VisualComplete,Started,Cancelled,Failed,SkippedNoVisual};
enum class VisualPolicy:std::uint8_t {Automatic,Required,NoVisual};
enum class Reason:std::uint8_t {None,SceneEnded,HistoryReplaced,ModelRetired,MissingProcessor,MissingView,ViewReplaced,QueueCancelled,InvalidPayload,EnqueueFailed};
struct Placement {int card=-1,lane=-1,order=-1;};
struct SwapPayload {Placement firstBefore,firstAfter,secondBefore,secondAfter;int firstOwner=-1,secondOwner=-1;bool ignoreTerrain=false,ignoreOccupancy=false;};
struct DiscardPayload {int card=-1,owner=-1,hand=-1,pile=-1;bool concealed=false;};
struct TransferPayload {int card=-1,from=-1,to=-1,oldIndex=-1,newIndex=-1;int owner=-1,guid=-1,order=-1;bool created=false,stockRecorded=false,faceDown=false,teamup=false,aquatic=false;};
struct VariablePayload {int card=-1;bool timed=false;std::string before,after;};
struct LevelPayload {int card=-1,oldEntry=-1,newEntry=-1,oldValue=0,newValue=0,cost=0,attack=0,health=0;};
enum class SelectionPhase:std::uint8_t {Requested,Options,Accepted,Rejected,Presented,Dismissed,Cancelled};
struct SelectionPayload {int effect=-1,group=-1,owner=-1;SelectionPhase phase=SelectionPhase::Requested;std::vector<int> entities,guids,chosen;std::vector<std::optional<int>> costs,attacks,maxHealths,damages;};
struct MetadataPayload {int card=-1;std::string field,before,after;};
struct PoolCreatedPayload {int pool=-1,owner=-1,guid=-1,before=0,after=0,requested=0,position=0;bool superpowers=false;};
// Domain facts supplement stock records; they never apply stock damage twice.
struct CombatPayload {std::string phase;int first=-1,second=-1,firstLane=-1,secondLane=-1;bool laneMode=false;std::vector<int> attackers,defenders;std::vector<std::optional<int>> amounts;std::vector<std::string> effectTypes;};
struct AbilityPayload {int card=-1,ability=-1,source=-1,duration=-1,operation=0;bool triggered=false,expired=false;std::string before,after;std::string encoding="native-grant-v1";};
struct DeathPayload {int card=-1,damageBefore=0,damageAfter=0,health=0;bool markerRemoved=false;};
struct DamageRulePayload {std::string rule,phase;int originalTarget=-1,target=-1,before=0,after=0;bool cancelled=false;};
struct HealingPayload {int card=-1,requested=0,restored=0,bonus=0;bool queued=false;};
struct PresentationPayload {std::string system,phase;std::vector<int> entities;float firstAngle=0,secondAngle=0,seconds=0;};
using Payload=std::variant<std::monostate,DiscardPayload,SwapPayload,TransferPayload,VariablePayload,LevelPayload,SelectionPayload,MetadataPayload,PoolCreatedPayload,CombatPayload,AbilityPayload,DeathPayload,DamageRulePayload,HealingPayload,PresentationPayload>;
struct Record {
    std::uint64_t serial=0,epoch=0,modelKey=0,historyGeneration=0,modelToken=0;
    std::uintptr_t model=0,history=0; // internal identities; never serialize addresses
    int stockHistoryAnchor=-1;
    std::uint64_t correlationId=0,parentActionId=0;
    Kind kind=Kind::StateChange;Stage stage=Stage::Observed;Delivery delivery=Delivery::Pending;
    Origin from=Origin::Unknown,to=Origin::Unknown;
    VisualPolicy visual=VisualPolicy::Automatic;Reason reason=Reason::None;
    int entityId=-1,ownerId=-1,sourceId=-1,effectId=-1;
    int fromZoneId=-1,toZoneId=-1,cardGuid=-1,amount=0,aux=0;
    Payload payload;
};
inline bool Required(const Record& r){return r.visual==VisualPolicy::Required || (r.visual==VisualPolicy::Automatic && (r.kind==Kind::DiscardOutsidePlay || r.kind==Kind::SwapTransaction));}
inline bool Terminal(Delivery d){return d==Delivery::VisualComplete || d==Delivery::Cancelled || d==Delivery::Failed || d==Delivery::SkippedNoVisual;}
struct Event {std::uint64_t sequence=0;Record record;std::uint64_t timeUs=0;};
}
