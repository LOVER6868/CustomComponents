#pragma once

#include <cstdint>

#include <BNM/UserSettings/GlobalSettings.hpp>
#include <BNM/Il2CppHeaders.hpp>

#include <mutex>
#include <string>
#include <vector>

namespace CustomComponents::Queries {

using Obj = BNM::IL2CPP::Il2CppObject;

struct QueryRegistration {
    const char* name;
    const char* serializedTypeName;
    const char* runtimeNamespace;
    const char* runtimeClass;
    const char* baseNamespace;
    const char* baseClass;
};

inline std::mutex queryRegistryMutex;
inline std::vector<QueryRegistration> queryRegistry;

inline bool SerializedTypeMatches(const std::string& serialized, const char* fullTypeName) {
    if (!fullTypeName || !*fullTypeName) return false;
    const std::string wanted(fullTypeName);
    if (serialized == wanted) return true;
    return serialized.size() > wanted.size() &&
           serialized.compare(0, wanted.size(), wanted) == 0 &&
           serialized[wanted.size()] == ',';
}

inline bool RegisterQuery(const QueryRegistration& registration) {
    if (!registration.name || !registration.serializedTypeName ||
        !registration.runtimeNamespace || !registration.runtimeClass ||
        !registration.baseNamespace || !registration.baseClass) {
        return false;
    }

    std::lock_guard<std::mutex> lock(queryRegistryMutex);
    for (const auto& existing : queryRegistry) {
        if (std::string(existing.serializedTypeName) == registration.serializedTypeName)
            return false;
    }
    queryRegistry.push_back(registration);
    return true;
}

inline const QueryRegistration* FindSerializedQuery(const std::string& serialized) {
    std::lock_guard<std::mutex> lock(queryRegistryMutex);
    for (const auto& registration : queryRegistry) {
        if (SerializedTypeMatches(serialized, registration.serializedTypeName))
            return &registration;
    }
    return nullptr;
}

} // namespace CustomComponents::Queries
