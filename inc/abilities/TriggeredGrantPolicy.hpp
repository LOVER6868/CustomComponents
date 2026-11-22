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
// Guid, source, duration, value type, amount, operation, base type, base amount.
using Row=std::array<int,8>;
inline std::vector<Row> Decode(std::string_view text){
    std::vector<Row> rows;std::size_t at=0;
    while(at<text.size()){
        Row row{};
        for(std::size_t i=0;i<row.size();++i){
            const auto parsed=std::from_chars(text.data()+at,text.data()+text.size(),row[i]);
            if(parsed.ec!=std::errc{})return {};
            at=static_cast<std::size_t>(parsed.ptr-text.data());
            if(at>=text.size() || text[at++]!=(i+1==row.size()?';':','))return {};
        }
        rows.push_back(row);if(rows.size()>4096)return {};
    }
    return rows;
}
inline std::string Encode(const std::vector<Row>& rows){
    std::string result;for(const auto& row:rows)for(std::size_t i=0;i<row.size();++i){result+=std::to_string(row[i]);result+=i+1==row.size()?';':',';}return result;
}
struct Value {int type=0,amount=0;};
inline Value Evaluate(const std::vector<Row>& rows,int guid,Value baseline){
    Value value=baseline;
    for(const auto& row:rows)if(row[0]==guid && row[2]>=0){
        if(row[5]==1)value={row[3],row[4]};
        else if(row[5]==2){
            const auto amount=std::int64_t(value.amount)+row[4];
            value={row[3],static_cast<int>(std::min(std::int64_t(std::numeric_limits<int>::max()),std::max(std::int64_t(std::numeric_limits<int>::min()),amount)))};
        }else if(value.amount<row[4])value={row[3],row[4]};
    }
    return value;
}
}