#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "core/CustomComponentRegistry.hpp"
#include "queries/QuerySupport.hpp"
#include "log.h"
namespace CustomComponents::Systems::ComponentSupport {

    using Obj = BNM::IL2CPP::Il2CppObject;

    // ============================================================================
    // CustomComponents module infrastructure
    //
    // main.cpp deliberately owns only framework-wide integration:
    //   1. custom serialized type resolution;
    //   2. generic construction of registered custom components;
    //   3. module startup.
    //
    // Trait-specific registration, validation, fields, hooks and gameplay belong
    // beside the component itself (for example components/Charge.hpp).
    // ============================================================================

    using GetTypeFromTypeStringFn =
        void* (*)(BNM::Structures::Mono::String* typeString, const void* method);
    using InstanceManagerCreateInstanceFn =
        void* (*)(void* type, const void* method);

    static GetTypeFromTypeStringFn originalGetTypeFromTypeString = nullptr;
    static InstanceManagerCreateInstanceFn originalCreateInstance = nullptr;
    // ----------------------------------------------------------------------------
    // Serialized type resolution
    // ----------------------------------------------------------------------------

    static void* GetTypeFromTypeStringHook(
        BNM::Structures::Mono::String* typeString,
        const void* method
    ) {
        const std::string text = typeString ? typeString->str() : std::string{};

        // Queries must still be intercepted before the stock resolver. Keep one
        // useful log only when a custom query was actually handled.
        auto queryResolution =
            CustomComponents::Queries::QuerySupport::TryResolveSerializedQuery(text);
        if (queryResolution.handled)
            return queryResolution.resolvedType;

        auto* component = CustomComponents::CustomComponentRegistry::FindSerializedType(text);
        if (!component)
            return originalGetTypeFromTypeString(typeString, method);

        return static_cast<void*>(component->monoType);
    }

    static bool InstallTypeResolver() {
        auto klass = BNM::Class(
            "PvZCards.Core.Serialization",
            "SerializedTypeGetter"
        );

        if (!klass.IsValid()) {
            LOG_ERR("CustomComponents: SerializedTypeGetter was not found");
            return false;
        }

        auto method = klass.GetMethod("GetTypeFromTypeString", 1);
        if (!method.IsValid()) {
            LOG_ERR("CustomComponents: GetTypeFromTypeString was not found");
            return false;
        }

        BNM::BasicHook(method, GetTypeFromTypeStringHook, originalGetTypeFromTypeString);

        if (!originalGetTypeFromTypeString) {
            LOG_ERR("CustomComponents: serialized type resolver hook failed");
            return false;
        }

        LOG_INFO("CustomComponents: serialized type resolver ready");
        return true;
    }


    // ----------------------------------------------------------------------------
    // Generic custom-component construction
    // ----------------------------------------------------------------------------

    static void* CreateInstanceHook(void* type, const void* method) {
        // Queries are handled first because QuerySupport owns the pending
        // serialized-query construction state.
        auto queryConstruction =
            CustomComponents::Queries::QuerySupport::ConsumePendingConstruction(
                type, method, originalCreateInstance
            );
        if (queryConstruction.handled)
            return queryConstruction.instance;

        auto* component = CustomComponents::CustomComponentRegistry::FindByType(type);
        if (!component)
            return originalCreateInstance(type, method);

        void* instance = CustomComponents::CustomComponentRegistry::CreateInstance(*component);
        if (!instance) {
            LOG_ERR(
                "CustomComponents/ComponentSupport: custom construction FAILED component='%s' type=%p",
                component->name, type
            );
            return nullptr;
        }

        return instance;
    }

    static bool InstallComponentConstructor() {
        auto klass = BNM::Class("PvZCards.Engine", "InstanceManager");
        if (!klass.IsValid()) {
            LOG_ERR("CustomComponents: InstanceManager was not found");
            return false;
        }

        auto method = klass.GetMethod("CreateInstance", 1);
        if (!method.IsValid()) {
            LOG_ERR("CustomComponents: InstanceManager.CreateInstance(Type) was not found");
            return false;
        }

        BNM::BasicHook(method, CreateInstanceHook, originalCreateInstance);

        if (!originalCreateInstance) {
            LOG_ERR("CustomComponents: custom-component constructor hook failed");
            return false;
        }

        LOG_INFO("CustomComponents: generic component constructor ready");
        return true;
    }



}