#pragma once
#include <array>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace CustomComponents::Abilities::TriggeredGrants::Policy {
// The saved ledger is version 1 and must remain eight comma-separated ints
// followed by ';'. Only the internal C++ representation is changed.
enum class Operation : std::int32_t {Maximum=0,Set=1,Add=2};
inline constexpr int ToNative(Operation value){return static_cast<int>(value);}
inline constexpr Operation ParseGrantOperation(std::string_view value){
    if(value=="Set" || value=="1")return Operation::Set;
    if(value=="Add" || value=="2")return Operation::Add;
    return Operation::Maximum; // omitted/invalid is handled by the descriptor parser
}
struct Row {
    int guid=0,source=0,duration=0,valueType=0,amount=0;
    Operation operation=Operation::Maximum;
    int baselineType=0,baselineAmount=0;
    Row()=default;
    constexpr Row(int g,int s,int d,int t,int a,int op,int bt,int ba)
        :guid(g),source(s),duration(d),valueType(t),amount(a),
         operation(static_cast<Operation>(op)),baselineType(bt),baselineAmount(ba){}
    constexpr Row(int g,int s,int d,int t,int a,Operation op,int bt,int ba)
        :guid(g),source(s),duration(d),valueType(t),amount(a),
         operation(op),baselineType(bt),baselineAmount(ba){}
    constexpr std::array<int,8> Wire() const {
        return {guid,source,duration,valueType,amount,ToNative(operation),baselineType,baselineAmount};
    }
    bool operator==(const Row&)const=default;
};
inline std::vector<Row> Decode(std::string_view text){
    std::vector<Row> rows;std::size_t at=0;
    while(at<text.size()){
        std::array<int,8> values{};
        for(std::size_t i=0;i<values.size();++i){
            const auto parsed=std::from_chars(text.data()+at,text.data()+text.size(),values[i]);
            if(parsed.ec!=std::errc{})return {};
            at=static_cast<std::size_t>(parsed.ptr-text.data());
            if(at>=text.size() || text[at++]!=(i+1==values.size()?';':','))return {};
        }
        rows.emplace_back(values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7]);
        if(rows.size()>4096)return {};
    }
    return rows;
}
inline std::string Encode(const std::vector<Row>& rows){
    std::string result;
    for(const auto& row:rows){
        const auto values=row.Wire();
        for(std::size_t i=0;i<values.size();++i){
            result+=std::to_string(values[i]);
            result+=i+1==values.size()?';':',';
        }
    }
    return result;
}
struct Value {int type=0,amount=0;};
inline Value Evaluate(const std::vector<Row>& rows,int guid,Value baseline){
    Value value=baseline;
    for(const auto& row:rows)if(row.guid==guid && row.duration>=0){
        switch(row.operation){
        case Operation::Set:value={row.valueType,row.amount};break;
        case Operation::Add:{
            const auto amount=std::int64_t(value.amount)+row.amount;
            value={row.valueType,static_cast<int>(std::min(std::int64_t(std::numeric_limits<int>::max()),std::max(std::int64_t(std::numeric_limits<int>::min()),amount)))};
            break;
        }
        default:if(value.amount<row.amount)value={row.valueType,row.amount};break;
        }
    }
    return value;
}
} // namespace CustomComponents::Abilities::TriggeredGrants::Policy
