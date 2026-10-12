#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Utils.hpp>

#include "log.h"

namespace CustomComponents::EnumSerializationSupport {

struct Entry {
    BNM::MonoType* type{};
    const char* name{};
    int value{};
};

inline std::array<Entry, 64> entries{};
inline std::size_t entryCount = 0;

using GetValueFn = void* (*)(void* type, void* value, const void* method);
inline GetValueFn originalGetValue = nullptr;

inline std::string Text(void* object) {
    if (!object)
        return {};

    BNM::Class klass(static_cast<BNM::IL2CPP::Il2CppObject*>(object));
    if (klass.GetClass() == BNM::Class("System", "String").GetClass())
        return static_cast<BNM::Structures::Mono::String*>(object)->str();

    auto toString = klass.GetMethod("ToString", 0)
        .cast<BNM::Structures::Mono::String*>();
    if (!toString.IsValid())
        return {};

    auto* string = toString[object]();
    return string ? string->str() : std::string{};
}

inline bool Register(BNM::MonoType* type, const char* name, int value) {
    if (!type || !name || !*name)
        return false;

    for (std::size_t i = 0; i < entryCount; ++i) {
        if (entries[i].type == type &&
            std::string_view(entries[i].name) == name) {
            return entries[i].value == value;
        }
    }

    if (entryCount == entries.size())
        return false;

    entries[entryCount++] = { type, name, value };
    return true;
}

static void* GetValueHook(void* type, void* value, const void* method) {
    if (!originalGetValue)
        return nullptr;

    if (!type || !value)
        return originalGetValue(type, value, method);

    bool registeredType = false;
    for (std::size_t i = 0; i < entryCount; ++i) {
        if (entries[i].type == type) {
            registeredType = true;
            break;
        }
    }

    if (!registeredType)
        return originalGetValue(type, value, method);

    const std::string text = Text(value);
    for (std::size_t i = 0; i < entryCount; ++i) {
        const auto& entry = entries[i];
        if (entry.type == type && text == entry.name) {
            const std::string numeric = std::to_string(entry.value);
            auto* numericString = BNM::CreateMonoString(numeric.c_str());
            return originalGetValue(type, numericString, method);
        }
    }

    return originalGetValue(type, value, method);
}

inline bool Initialize() {
    auto defaultDeserializer = BNM::Class(
        "PvZCards.Core.Serialization",
        "DefaultDeserializer"
    );
    if (!defaultDeserializer.IsValid()) {
        LOG_ERR("CustomComponents/EnumSerialization: DefaultDeserializer missing");
        return false;
    }

    auto getValue = defaultDeserializer.GetMethod("GetValue", 2);
    if (!getValue.IsValid()) {
        LOG_ERR("CustomComponents/EnumSerialization: GetValue(Type, object) missing");
        return false;
    }

    BNM::BasicHook(getValue, GetValueHook, originalGetValue);
    if (!originalGetValue) {
        LOG_ERR("CustomComponents/EnumSerialization: GetValue hook failed");
        return false;
    }

    return true;
}

} // namespace CustomComponents::EnumSerializationSupport
