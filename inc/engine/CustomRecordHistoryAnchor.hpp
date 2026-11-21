#pragma once
// Reads stock History.Records.Count as a presentation ordering watermark.
// Null/unavailable history returns -1: custom frontend processing must wait
// for stock history to be completely caught up before accepting the record.
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
namespace CustomComponents::Engine::CustomRecordHistoryAnchor {
using Obj=BNM::IL2CPP::Il2CppObject;
inline int Count(Obj* history) {
    static auto field=BNM::Class("PvZCards.Engine","History").GetField("Records")
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    auto* list=history && field.IsValid()?field[history].Get():nullptr;
    return list && list->size>=0 && list->size<1000000 ? list->size : -1;
}
}
