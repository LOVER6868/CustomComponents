#pragma once
#include "queries/LaneQueries.hpp"
#include "core/NativeConstruction.hpp"
#include "core/HookRegistry.hpp"
#include <cstring>

namespace CustomComponents::Queries::CheatSelection {
using List = BNM::Structures::Mono::List<Obj*>;
inline BNM::Field<List*> children;
inline BNM::Field<Obj*> subset;
inline void (*originalExclude)(void*, Obj*, Obj*, const void*) = nullptr;

// This is an opt-in annotation in an AND query. It does not change card tags
// or bypass the other subset requirements, such as usability and aquatic lanes.
struct IgnoreCheatTagQuery : Obj {
    BNM_CustomClass(IgnoreCheatTagQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "IgnoreCheatTagQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    void Construct() { Core::NativeConstruction::Base(this); }
    bool Matches(Obj*, Obj*) { return true; }
    BNM_CustomMethod(Construct, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
};
inline bool OptedIn(Obj* query, unsigned depth = 0) {
    if (!query || !query->klass || !query->klass->name || depth > 32) return false;
    const std::string_view name(query->klass->name);
    if (name == "IgnoreCheatTagQuery") return true;
    // An OR branch or a negation must not silently widen the entire search.
    if (name != "CompositeAllQuery") return false;
    const auto* list = Core::Metadata::Bind(children, query).Get();
    if (!Engine::EntityAccess::Valid(list)) return false;
    for (int i = 0; i < list->size; ++i)
        if (OptedIn(list->items->m_Items[i], depth + 1)) return true;
    return false;
}
static void Exclude(void* criteria, Obj* instances, Obj* query, const void* method) {
    // This hook receives an unboxed value type. Binding it as a managed object
    // would run BNM's instance-class check on its first payload pointer.
    // GetOffset already accounts for the absent object header.
    Obj* authored=nullptr;
    if(criteria)std::memcpy(&authored,static_cast<const char*>(criteria)+subset.GetOffset(),sizeof(authored));
    if (!OptedIn(authored)) originalExclude(criteria, instances, query, method);
}
inline bool Initialize() {
    const auto type = BNM::Class("PvZCards.Engine.Queries", "IgnoreCheatTagQuery");
    const auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    children = BNM::Class("PvZCards.Engine.Queries", "CompositeQuery").GetField("Queries").cast<List*>();
    const auto criteria = BNM::Class("PvZCards.Core.Systems.Utilities", "CardDataCriteria");
    subset = criteria.GetField("SubsetQuery").cast<Obj*>();
    return type.IsValid() && base.IsValid() && children.IsValid() && subset.IsValid() &&
        PatchInDeckQueryDispatch(type, base, "IgnoreCheatTagQuery") &&
        RegisterQuery({"IgnoreCheatTagQuery", "PvZCards.Engine.Queries.IgnoreCheatTagQuery",
            "PvZCards.Engine.Queries", "IgnoreCheatTagQuery", "PvZCards.Engine.Queries", "InHandQuery"}) &&
        Core::Hooks::Install(criteria.GetMethod("AddNotCheatsQuery", 2), Exclude, originalExclude);
}
}
