#pragma once
#include <string>
#include <cmath>
#include "history/CustomHistory.hpp"
#include "history/KindNames.hpp"
namespace CustomComponents::History::Wire {
inline std::string Quote(const std::string& text){std::string out="\"";for(unsigned char c:text){switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(c<32){const char* hex="0123456789abcdef";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}}return out+'"';}
inline std::string Numbers(const std::vector<int>& values){std::string out="[";for(int v:values){if(out.size()>1)out+=',';out+=std::to_string(v);}return out+"]";}
inline std::string Position(const Placement& p){return "{\"card\":"+std::to_string(p.card)+",\"lane\":"+std::to_string(p.lane)+",\"order\":"+std::to_string(p.order)+"}";}
inline std::string OptionalNumbers(const std::vector<std::optional<int>>& values){
    std::string out="[";
    for(auto value:values){if(out.size()>1)out+=',';out+=value?std::to_string(*value):"null";}
    return out+"]";
}
struct Object {
    std::string text="{";
    void Add(const char* key,const std::string& value){if(text.size()>1)text+=',';text+=Quote(key)+":"+Quote(value);}
    void Add(const char* key,int value){Raw(key,std::to_string(value));}
    void Add(const char* key,float value){Raw(key,std::isfinite(value)?std::to_string(value):"null");}
    void Add(const char* key,bool value){Raw(key,value?"true":"false");}
    void Add(const char* key,const std::vector<int>& values){Raw(key,Numbers(values));}
    void Add(const char* key,const std::vector<std::optional<int>>& values){Raw(key,OptionalNumbers(values));}
    void Add(const char* key,const std::vector<std::string>& values){
        std::string encoded="[";for(const auto& value:values){if(encoded.size()>1)encoded+=',';encoded+=Quote(value);}Raw(key,encoded+"]");
    }
    void Raw(const char* key,const std::string& value){if(text.size()>1)text+=',';text+=Quote(key)+":"+value;}
    std::string Finish(){return text+"}";}
};
inline std::string PayloadJson(const Payload& payload){return std::visit([](const auto& p)->std::string{
    using T=std::decay_t<decltype(p)>;
    if constexpr(std::is_same_v<T,std::monostate>)return "{}";
    else if constexpr(std::is_same_v<T,SwapPayload>){
        if(!p.moves.empty()){
            std::string out="{\"moves\":[";
            for(const auto& move:p.moves){
                if(out.back()!='[')out+=',';
                out+="{\"before\":"+Position(move.before)+",\"after\":"+Position(move.after)+",\"owner\":"+std::to_string(move.owner)+"}";
            }
            return out+"]}";
        }
        return "{\"firstBefore\":"+Position(p.firstBefore)+",\"firstAfter\":"+Position(p.firstAfter)+",\"secondBefore\":"+Position(p.secondBefore)+",\"secondAfter\":"+Position(p.secondAfter)+",\"firstOwner\":"+std::to_string(p.firstOwner)+",\"secondOwner\":"+std::to_string(p.secondOwner)+",\"ignoreTerrain\":"+(p.ignoreTerrain?"true":"false")+",\"ignoreOccupancy\":"+(p.ignoreOccupancy?"true":"false")+"}";}
    else if constexpr(std::is_same_v<T,DiscardPayload>)return "{\"card\":"+std::to_string(p.card)+",\"owner\":"+std::to_string(p.owner)+",\"hand\":"+std::to_string(p.hand)+",\"pile\":"+std::to_string(p.pile)+",\"concealed\":"+(p.concealed?"true":"false")+"}";
    else if constexpr(std::is_same_v<T,CombatPayload>){Object o;o.Add("phase",p.phase);o.Add("first",p.first);o.Add("second",p.second);o.Add("firstLane",p.firstLane);o.Add("secondLane",p.secondLane);o.Add("laneMode",p.laneMode);o.Add("attackers",p.attackers);o.Add("defenders",p.defenders);o.Add("amounts",p.amounts);o.Add("effectTypes",p.effectTypes);return o.Finish();}
    else if constexpr(std::is_same_v<T,AbilityPayload>){Object o;o.Add("card",p.card);o.Add("ability",p.ability);o.Add("source",p.source);o.Add("duration",p.duration);o.Add("operation",p.operation);o.Add("triggered",p.triggered);o.Add("expired",p.expired);o.Add("encoding",p.encoding);o.Add("before",p.before);o.Add("after",p.after);return o.Finish();}
    else if constexpr(std::is_same_v<T,DeathPayload>){Object o;o.Add("card",p.card);o.Add("damageBefore",p.damageBefore);o.Add("damageAfter",p.damageAfter);o.Add("health",p.health);o.Add("markerRemoved",p.markerRemoved);return o.Finish();}
    else if constexpr(std::is_same_v<T,DamageRulePayload>){Object o;o.Add("rule",p.rule);o.Add("phase",p.phase);o.Add("originalTarget",p.originalTarget);o.Add("target",p.target);o.Add("before",p.before);o.Add("after",p.after);o.Add("cancelled",p.cancelled);return o.Finish();}
    else if constexpr(std::is_same_v<T,HealingPayload>){Object o;o.Add("card",p.card);o.Add("requested",p.requested);o.Add("restored",p.restored);o.Add("bonus",p.bonus);o.Add("queued",p.queued);return o.Finish();}
    else if constexpr(std::is_same_v<T,TransformDonorPayload>){Object o;o.Add("donor",p.donor);o.Add("target",p.target);o.Add("lane",p.lane);return o.Finish();}
    else if constexpr(std::is_same_v<T,PresentationPayload>){Object o;o.Add("system",p.system);o.Add("phase",p.phase);o.Add("entities",p.entities);o.Add("firstAngle",p.firstAngle);o.Add("secondAngle",p.secondAngle);o.Add("seconds",p.seconds);return o.Finish();}
    else if constexpr(std::is_same_v<T,TransferPayload>){Object o;o.Add("card",p.card);o.Add("from",p.from);o.Add("to",p.to);o.Add("oldIndex",p.oldIndex);o.Add("newIndex",p.newIndex);o.Add("owner",p.owner);o.Add("guid",p.guid);o.Add("order",p.order);o.Add("created",p.created);o.Add("stockRecorded",p.stockRecorded);o.Add("faceDown",p.faceDown);o.Add("teamup",p.teamup);o.Add("aquatic",p.aquatic);return o.Finish();}
    else if constexpr(std::is_same_v<T,VariablePayload>)return "{\"card\":"+std::to_string(p.card)+",\"timed\":"+(p.timed?"true":"false")+",\"before\":"+Quote(p.before)+",\"after\":"+Quote(p.after)+"}";
    else if constexpr(std::is_same_v<T,LevelPayload>)return "{\"card\":"+std::to_string(p.card)+",\"oldEntry\":"+std::to_string(p.oldEntry)+",\"newEntry\":"+std::to_string(p.newEntry)+",\"oldValue\":"+std::to_string(p.oldValue)+",\"newValue\":"+std::to_string(p.newValue)+",\"costBonus\":"+std::to_string(p.cost)+",\"attackBonus\":"+std::to_string(p.attack)+",\"healthBonus\":"+std::to_string(p.health)+"}";
    else if constexpr(std::is_same_v<T,SelectionPayload>)return "{\"effect\":"+std::to_string(p.effect)+",\"group\":"+std::to_string(p.group)+",\"owner\":"+std::to_string(p.owner)+",\"phase\":"+std::to_string(unsigned(p.phase))+",\"entities\":"+Numbers(p.entities)+",\"guids\":"+Numbers(p.guids)+",\"chosen\":"+Numbers(p.chosen)+",\"costs\":"+OptionalNumbers(p.costs)+",\"attacks\":"+OptionalNumbers(p.attacks)+",\"maxHealths\":"+OptionalNumbers(p.maxHealths)+",\"damages\":"+OptionalNumbers(p.damages)+"}";
    else if constexpr(std::is_same_v<T,PoolCreatedPayload>)return "{\"pool\":"+std::to_string(p.pool)+",\"owner\":"+std::to_string(p.owner)+",\"guid\":"+std::to_string(p.guid)+",\"before\":"+std::to_string(p.before)+",\"after\":"+std::to_string(p.after)+",\"requested\":"+std::to_string(p.requested)+",\"position\":"+std::to_string(p.position)+",\"superpowers\":"+(p.superpowers?"true":"false")+"}";
    else return "{\"card\":"+std::to_string(p.card)+",\"field\":"+Quote(p.field)+",\"before\":"+Quote(p.before)+",\"after\":"+Quote(p.after)+"}";
},payload);}
inline std::string Serialize(const Event& e){const auto& r=e.record;
    return "{\"schemaVersion\":2,\"sequence\":"+std::to_string(e.sequence)+",\"timeUs\":"+std::to_string(e.timeUs)+",\"recordId\":"+std::to_string(r.serial)+",\"epoch\":"+std::to_string(r.epoch)+",\"modelKey\":"+std::to_string(r.modelKey)+",\"historyGeneration\":"+std::to_string(r.historyGeneration)+",\"stockBoundary\":"+std::to_string(r.stockHistoryAnchor)+",\"correlationId\":"+std::to_string(r.correlationId)+",\"parentActionId\":"+std::to_string(r.parentActionId)+",\"kind\":"+std::to_string(unsigned(r.kind))+",\"kindName\":"+Quote(KindName(r.kind))+",\"delivery\":"+std::to_string(unsigned(r.delivery))+",\"stage\":"+std::to_string(unsigned(r.stage))+",\"visualPolicy\":"+std::to_string(unsigned(r.visual))+",\"reason\":"+std::to_string(unsigned(r.reason))+",\"entityId\":"+std::to_string(r.entityId)+",\"ownerId\":"+std::to_string(r.ownerId)+",\"sourceId\":"+std::to_string(r.sourceId)+",\"effectId\":"+std::to_string(r.effectId)+",\"from\":"+std::to_string(unsigned(r.from))+",\"to\":"+std::to_string(unsigned(r.to))+",\"payload\":"+PayloadJson(r.payload)+"}";
}
}
