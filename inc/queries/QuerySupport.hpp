#pragma once

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>

#include <string>

#include "queries/QueryRegistry.hpp"
#include "log.h"

namespace CustomComponents::Queries::QuerySupport {

struct ResolveResult { bool handled = false; void* resolvedType = nullptr; };
struct ConstructResult { bool handled = false; void* instance = nullptr; };

inline thread_local bool hasPendingConstruction = false;
inline thread_local QueryRegistration pendingConstruction{};

inline ResolveResult TryResolveSerializedQuery(const std::string& serialized) {
    const auto* registration = FindSerializedQuery(serialized);
    if (!registration) return {};

    auto runtimeClass = BNM::Class(registration->runtimeNamespace, registration->runtimeClass);
    if (!runtimeClass.IsValid() || !runtimeClass.GetMonoType()) {
        LOG_ERR(
            "CustomComponents/QuerySupport: resolve failed query='%s' runtime='%s.%s'",
            registration->name, registration->runtimeNamespace, registration->runtimeClass
        );
        return {true, nullptr};
    }

    pendingConstruction = *registration;
    hasPendingConstruction = true;

    void* type = static_cast<void*>(runtimeClass.GetMonoType());
    return {true, type};
}

template <typename CreateFn>
inline ConstructResult ConsumePendingConstruction(
    void* type,
    const void* /*method*/,
    CreateFn /*originalCreateInstance*/
) {
    if (!hasPendingConstruction) return {};

    QueryRegistration registration = pendingConstruction;
    hasPendingConstruction = false;
    pendingConstruction = {};

    auto runtimeClass = BNM::Class(registration.runtimeNamespace, registration.runtimeClass);
    auto baseClass = BNM::Class(registration.baseNamespace, registration.baseClass);
    if (!runtimeClass.IsValid() || !baseClass.IsValid()) {
        LOG_ERR("CustomComponents/QuerySupport: construction class lookup failed query='%s'",
                registration.name);
        return {true, nullptr};
    }

    // Match the proven CustomComponentRegistry construction pattern:
    // allocate the BNM subclass, then invoke the real concrete stock base .ctor.
    void* instance = runtimeClass.CreateNewInstance();
    if (!instance) {
        LOG_ERR("CustomComponents/QuerySupport: allocation failed query='%s' type=%p",
                registration.name, type);
        return {true, nullptr};
    }

    auto ctor = baseClass.GetMethod(".ctor", 0);
    if (!ctor.IsValid()) {
        LOG_ERR("CustomComponents/QuerySupport: base ctor missing query='%s' base='%s.%s'",
                registration.name, registration.baseNamespace, registration.baseClass);
        return {true, nullptr};
    }

    ctor.cast<void>()[instance]();

    return {true, instance};
}

} // namespace CustomComponents::Queries::QuerySupport
