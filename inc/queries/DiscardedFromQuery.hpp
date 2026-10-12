#pragma once

#include <cstdint>
#include <atomic>

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "queries/InDeckQuery.hpp"
#include "components/Discarded.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"
#include "log.h"

namespace CustomComponents::Queries {

inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> discardedFromComponents;
inline BNM::Field<std::int32_t> discardedFromSourceField;
inline BNM::IL2CPP::Il2CppClass* discardedFromComponentClass = nullptr;

// Source: 1 = Hand, 2 = Deck. Source=0 (missing/unknown) never matches.
// This tests provenance, NOT current discard pile membership.
struct DiscardedFromQuery final : BNM::IL2CPP::Il2CppObject {
    std::int32_t Source = 0;

    void Constructor() {
        auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
        auto ctor = base.GetMethod(".ctor", 0);
        if (ctor.IsValid()) ctor.cast<void>()[this]();
    }

    bool Matches(Obj* entity, Obj* /*context*/) {
        // Diagnostic-only: bounded to prevent per-frame log flooding.
        static unsigned diagnosticCalls = 0;
        const unsigned call = ++diagnosticCalls;
        const bool logThis = call <= 160;
        if (logThis) LOG_INFO(
            "CustomComponents/DiscardedFromQuery/DIAG: ENTER call=%u query=%p entity=%p expectedSource=%d componentsField=%d sourceField=%d markerClass=%p",
            call, this, entity, static_cast<int>(Source),
            discardedFromComponents.IsValid() ? 1 : 0,
            discardedFromSourceField.IsValid() ? 1 : 0,
            discardedFromComponentClass);
        if (!entity || (Source != 1 && Source != 2) ||
            !discardedFromComponents.IsValid() ||
            !discardedFromSourceField.IsValid() || !discardedFromComponentClass) {
            if (logThis) LOG_INFO("CustomComponents/DiscardedFromQuery/DIAG: REJECT call=%u reason=invalid-input-or-source", call);
            return false;
        }

        auto* components = discardedFromComponents[entity].Get();
        if (!components || !components->items || components->size < 0 || components->size > 256) {
            if (logThis) LOG_INFO("CustomComponents/DiscardedFromQuery/DIAG: REJECT call=%u reason=invalid-components list=%p", call, components);
            return false;
        }
        auto** items = components->GetData();
        if (!items) {
            if (logThis) LOG_INFO("CustomComponents/DiscardedFromQuery/DIAG: REJECT call=%u reason=null-items", call);
            return false;
        }
        for (int i = 0; i < components->size; ++i) {
            Obj* component = items[i];
            if (component && component->klass == discardedFromComponentClass) {
                const std::int32_t actual = discardedFromSourceField[component].Get();
                const bool match = actual == Source;
                if (logThis) LOG_INFO(
                    "CustomComponents/DiscardedFromQuery/DIAG: RESULT call=%u marker=%p actualSource=%d expectedSource=%d match=%d",
                    call, component, static_cast<int>(actual), static_cast<int>(Source), match ? 1 : 0);
                return match;
            }
        }
        if (logThis) LOG_INFO("CustomComponents/DiscardedFromQuery/DIAG: REJECT call=%u reason=marker-not-found componentCount=%d", call, components->size);
        return false;
    }

    BNM_CustomClass(
        DiscardedFromQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "DiscardedFromQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(),
        {}
    );
    void DeepCopyFrom(Obj* source, Obj* manager) {
        auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
        auto copy = base.GetMethod("DeepCopyFrom", 2);
        if (copy.IsValid()) copy.cast<void>()[this](source, manager);
        auto field = BNM::Class("PvZCards.Engine.Queries", "DiscardedFromQuery")
            .GetField("Source").cast<std::int32_t>();
        Source = (source && field.IsValid()) ? field[source].Get() : 0;
        LOG_INFO("CustomComponents/DiscardedFromQuery/COPY: source=%p target=%p Source=%d",
                 source, this, static_cast<int>(Source));
    }

    BNM_CustomField(Source, BNM::Defaults::Get<std::int32_t>(), "Source");
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(
        Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build()
    );
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(DeepCopyFrom);
};

// Repair the inherited concrete and DeepCopyable interface dispatch entries.
// The stock InHandQuery DeepCopyFrom is stateless, so leaving either slot
// inherited silently drops our Source value during EntityQueryUtils.DeepCopy.
inline bool PatchDiscardedCopyDispatch(const BNM::Class& customClass,
                                       const BNM::Class& baseClass) {
    auto deepCopyable = BNM::Class("PvZCards.Engine", "DeepCopyable");
    auto customMethod = customClass.GetMethod("DeepCopyFrom", 2);
    auto baseMethod = baseClass.GetMethod("DeepCopyFrom", 2);
    auto interfaceMethod = deepCopyable.GetMethod("DeepCopyFrom", 2);
    auto* klass = customClass.GetClass();
    auto* customInfo = customMethod.IsValid() ? customMethod.GetInfo() : nullptr;
    auto* baseInfo = baseMethod.IsValid() ? baseMethod.GetInfo() : nullptr;
    auto* interfaceInfo = interfaceMethod.IsValid() ? interfaceMethod.GetInfo() : nullptr;
    if (!klass || !deepCopyable.IsValid() || !customInfo || !baseInfo || !interfaceInfo) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/COPY: dispatch metadata missing klass=%p custom=%p base=%p iface=%p",
                klass, customInfo, baseInfo, interfaceInfo);
        return false;
    }

    const uint16_t concreteSlot = baseInfo->slot;
    if (concreteSlot >= klass->vtable_count) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/COPY: invalid concrete slot=%u count=%u",
                (unsigned)concreteSlot, (unsigned)klass->vtable_count);
        return false;
    }

    int32_t interfaceSlot = -1;
    for (uint16_t i = 0; i < klass->interface_offsets_count; ++i) {
        const auto& pair = klass->interfaceOffsets[i];
        if (pair.interfaceType != deepCopyable.GetClass()) continue;
        interfaceSlot = pair.offset + static_cast<int32_t>(interfaceInfo->slot);
        break;
    }
    if (interfaceSlot < 0 || interfaceSlot >= klass->vtable_count) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/COPY: DeepCopyable slot unresolved slot=%d count=%u",
                interfaceSlot, (unsigned)klass->vtable_count);
        return false;
    }

    customInfo->slot = concreteSlot;
    klass->vtable[concreteSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[concreteSlot].method = customInfo;
    klass->vtable[interfaceSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[interfaceSlot].method = customInfo;
    LOG_INFO("CustomComponents/DiscardedFromQuery/COPY: dispatch patched concrete=%u interface=%d class=%p method=%p",
             (unsigned)concreteSlot, interfaceSlot, klass, customInfo->methodPointer);
    return true;
}

// Read-only trace of the specialized nested-query deserializer.
// This hook is separate from the already-owned ReferenceTypeDeserializer hooks.
using CompositeDeserializeFn = void (*)(void*, Obj*, Obj*, const void*);
inline CompositeDeserializeFn originalCompositeDeserialize = nullptr;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> compositeTraceQueries;
inline BNM::Field<std::int32_t> compositeTraceSource;
inline BNM::IL2CPP::Il2CppClass* compositeTraceQueryClass = nullptr;
inline std::atomic<unsigned> compositeTraceCalls{0};

inline void TraceCompositeChildren(Obj* object, const char* phase, unsigned call) {
    if (!object || !compositeTraceQueries.IsValid() || !compositeTraceSource.IsValid()) return;
    auto* children = compositeTraceQueries[object].Get();
    if (!children || !children->items || children->size < 0 || children->size > 512) return;
    unsigned matched = 0;
    for (int i = 0; i < children->size; ++i) {
        Obj* child = children->items->m_Items[i];
        if (!child || child->klass != compositeTraceQueryClass) continue;
        ++matched;
        LOG_INFO("CustomComponents/DiscardedFromQuery/RVA: composite %s call=%u parent=%p index=%d child=%p Source=%d",
            phase, call, object, i, child, (int)compositeTraceSource[child].Get());
    }
    if (matched) LOG_INFO("CustomComponents/DiscardedFromQuery/RVA: composite %s call=%u matches=%u count=%d",
        phase, call, matched, children->size);
}

static void CompositeDeserializeTrace(void* self, Obj* object, Obj* properties, const void* method) {
    unsigned call = ++compositeTraceCalls;
    if (call <= 120) TraceCompositeChildren(object, "before", call);
    originalCompositeDeserialize(self, object, properties, method);
    if (call <= 120) TraceCompositeChildren(object, "after", call);
}

inline bool InitializeDiscardedFromQuery() {
    auto query = BNM::Class("PvZCards.Engine.Queries", "DiscardedFromQuery");
    auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto marker = BNM::Class("PvZCards.Engine.Components", "Discarded");
    if (!query.IsValid() || !base.IsValid() || !entity.IsValid() ||
        !marker.IsValid()) return false;

    discardedFromComponents = entity.GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
    discardedFromSourceField = marker.GetField("Source").cast<std::int32_t>();
    discardedFromComponentClass = marker.GetClass();
    if (!discardedFromComponents.IsValid() || !discardedFromSourceField.IsValid() ||
        !PatchInDeckQueryDispatch(query, base, "DiscardedFromQuery") ||
        !PatchDiscardedCopyDispatch(query, base)) return false;

    if (!CustomComponents::Systems::CustomFieldBridgeSystem::Register(
        "PvZCards.Engine.Queries", "DiscardedFromQuery",
        "PvZCards.Engine.Queries", "InHandQuery",
        {{"Source", CustomComponents::Systems::CustomFieldBridgeSystem::FieldKind::Int32}})) return false;

    auto compositeClass = BNM::Class("PvZCards.Engine.Queries", "CompositeQuery");
    auto compositeDeserializer = BNM::Class("PvZCards.Core.Serialization.Queries", "CompositeQueryDeserializer");
    if (!compositeClass.IsValid() || !compositeDeserializer.IsValid()) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/RVA: composite metadata missing");
        return false;
    }
    compositeTraceQueries = compositeClass.GetField("Queries").cast<BNM::Structures::Mono::List<Obj*>*>();
    compositeTraceSource = query.GetField("Source").cast<std::int32_t>();
    compositeTraceQueryClass = query.GetClass();
    auto compositeDeserialize = compositeDeserializer.GetMethod("Deserialize", 2);
    if (!compositeTraceQueries.IsValid() || !compositeTraceSource.IsValid() || !compositeDeserialize.IsValid()) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/RVA: composite diagnostics metadata unresolved");
        return false;
    }
    BNM::BasicHook(compositeDeserialize, CompositeDeserializeTrace, originalCompositeDeserialize);
    if (!originalCompositeDeserialize) {
        LOG_ERR("CustomComponents/DiscardedFromQuery/RVA: composite hook failed");
        return false;
    }
    LOG_INFO("CustomComponents/DiscardedFromQuery/RVA: CompositeQueryDeserializer.Deserialize hook installed (expected RVA=0x222292C)");

    if (!RegisterQuery({"DiscardedFromQuery", "PvZCards.Engine.Queries.DiscardedFromQuery",
                        "PvZCards.Engine.Queries", "DiscardedFromQuery",
                        "PvZCards.Engine.Queries", "InHandQuery"})) return false;
    LOG_INFO("CustomComponents/DiscardedFromQuery: ready (scoped Source strings, custom query copy+serializer)");
    return true;
}

} // namespace CustomComponents::Queries
