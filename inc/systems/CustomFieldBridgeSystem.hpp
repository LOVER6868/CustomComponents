#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/FieldBase.hpp>
#include <BNM/Method.hpp>
#include <BNM/MethodBase.hpp>

#include <dlfcn.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "log.h"

namespace CustomComponents::Systems::CustomFieldBridgeSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    using MonoString = BNM::Structures::Mono::String;

    enum class FieldKind : std::uint8_t {
        Bool,
        Int32,
        Float,
        String
    };

    struct FieldSpec {
        const char* name;
        FieldKind kind;
    };

    struct ResolvedField {
        std::string name;
        FieldKind kind{};
        std::uintptr_t offset{};
    };

    struct Schema {
        std::string namespaze;
        std::string typeName;
        std::string baseNamespace;
        std::string baseTypeName;
        bool preserveLifecycle = true;
        BNM::IL2CPP::Il2CppClass* klass = nullptr;
        std::vector<ResolvedField> fields;
    };

    inline std::vector<Schema> schemas;
    inline bool initialized = false;

    inline bool Register(
        const char* namespaze,
        const char* typeName,
        const char* baseNamespace,
        const char* baseTypeName,
        std::initializer_list<FieldSpec> fields,
        bool preserveLifecycle = true
    ) {
        if (!namespaze || !typeName || !baseNamespace || !baseTypeName)
            return false;

        for (const auto& existing : schemas) {
            if (existing.namespaze == namespaze && existing.typeName == typeName)
                return true;
        }

        Schema schema;
        schema.namespaze = namespaze;
        schema.typeName = typeName;
        schema.baseNamespace = baseNamespace;
        schema.baseTypeName = baseTypeName;
        schema.preserveLifecycle = preserveLifecycle;
        for (const auto& field : fields) {
            if (!field.name || !*field.name)
                return false;
            schema.fields.push_back({field.name, field.kind, 0});
        }
        schemas.push_back(std::move(schema));
        return true;
    }

    inline Schema* FindSchema(Obj* object) {
        if (!object)
            return nullptr;
        auto* klass = BNM::Class(object).GetClass();
        if (!klass)
            return nullptr;
        for (auto& schema : schemas) {
            if (schema.klass == klass)
                return &schema;
        }
        return nullptr;
    }

    struct CapturedValue {
        bool present = false;
        FieldKind kind{};
        union {
            bool boolValue;
            std::int32_t intValue;
            float floatValue;
            MonoString* stringValue;
        } value{};
    };

    inline void WriteValue(Obj* object, const ResolvedField& field, const CapturedValue& captured) {
        if (!object || !captured.present || !field.offset)
            return;
        auto* address = reinterpret_cast<std::uint8_t*>(object) + field.offset;
        switch (field.kind) {
            case FieldKind::Bool:
                *reinterpret_cast<bool*>(address) = captured.value.boolValue;
                break;
            case FieldKind::Int32:
                *reinterpret_cast<std::int32_t*>(address) = captured.value.intValue;
                break;
            case FieldKind::Float:
                *reinterpret_cast<float*>(address) = captured.value.floatValue;
                break;
            case FieldKind::String:
                *reinterpret_cast<MonoString**>(address) = captured.value.stringValue;
                break;
        }
    }

    inline CapturedValue ReadValue(Obj* object, const ResolvedField& field) {
        CapturedValue captured;
        captured.present = object && field.offset;
        captured.kind = field.kind;
        if (!captured.present)
            return captured;
        auto* address = reinterpret_cast<std::uint8_t*>(object) + field.offset;
        switch (field.kind) {
            case FieldKind::Bool:
                captured.value.boolValue = *reinterpret_cast<bool*>(address);
                break;
            case FieldKind::Int32:
                captured.value.intValue = *reinterpret_cast<std::int32_t*>(address);
                break;
            case FieldKind::Float:
                captured.value.floatValue = *reinterpret_cast<float*>(address);
                break;
            case FieldKind::String:
                captured.value.stringValue = *reinterpret_cast<MonoString**>(address);
                break;
        }
        return captured;
    }

    inline void ResetFields(Schema& schema, Obj* object) {
        if (!object)
            return;
        for (const auto& field : schema.fields) {
            CapturedValue zero{};
            zero.present = true;
            zero.kind = field.kind;
            zero.value.stringValue = nullptr;
            WriteValue(object, field, zero);
        }
    }

    using RefDeserializeIntoFn = void (*)(void*, void*, void*, const void*);
    inline RefDeserializeIntoFn originalRefDeserializeInto = nullptr;
    inline BNM::Method<Obj*> convertPropertiesToDictionary;
    inline void* (*objectUnbox)(Obj*) = nullptr;
    inline Obj* (*objectBox)(BNM::IL2CPP::Il2CppClass*, void*) = nullptr;

    // Convert named discard origins before stock SetObjectProperties attempts
    // System.Int32 conversion. Never touch values on unrelated objects.
    using SetObjectPropertiesFn = void (*)(void*, Obj*, Obj*, const void*);
    inline SetObjectPropertiesFn originalSetObjectProperties = nullptr;
    // Single native-hook owner. Consumers may register a pre-stock callback.
    // The callback MUST forward to the original setter exactly once.
    inline SetObjectPropertiesFn delegatedSetObjectProperties = nullptr;
    inline bool RegisterSetObjectPropertiesDelegate(SetObjectPropertiesFn callback) {
        if (!initialized || !originalSetObjectProperties || !callback ||
            delegatedSetObjectProperties) return false;
        delegatedSetObjectProperties = callback;
        return true;
    }
    inline SetObjectPropertiesFn GetOriginalSetObjectProperties() {
        return initialized ? originalSetObjectProperties : nullptr;
    }

    inline BNM::IL2CPP::Il2CppClass* int32Class = nullptr;


    inline Obj* DictionaryValue(Obj* properties, const char* name) {
        if (!properties || !name)
            return nullptr;
        BNM::Class dict(properties);
        auto contains = dict.GetMethod("ContainsKey", 1).cast<bool>();
        auto item = dict.GetMethod("get_Item", 1).cast<Obj*>();
        if (!contains.IsValid() || !item.IsValid())
            return nullptr;
        auto* key = BNM::CreateMonoString(name);
        if (!key || !contains[properties](key))
            return nullptr;
        return item[properties](key);
    }

    inline MonoString* ObjectToString(Obj* value) {
        if (!value)
            return nullptr;
        auto* klass = BNM::Class(value).GetClass();
        if (klass && klass->name && std::strcmp(klass->name, "String") == 0)
            return reinterpret_cast<MonoString*>(value);
        auto toString = BNM::Class(value).GetMethod("ToString", 0).cast<MonoString*>();
        return toString.IsValid() ? toString[value]() : nullptr;
    }

    static void SetObjectPropertiesHook(void* self, Obj* instance, Obj* properties, const void* method) {
        if (instance && properties && objectBox && int32Class) {
            Schema* schema = FindSchema(instance);
            if (schema && schema->namespaze == "PvZCards.Engine.Queries" &&
                schema->typeName == "DiscardedFromQuery") {
                Obj* source = DictionaryValue(properties, "Source");
                auto* sourceClass = source ? BNM::Class(source).GetClass() : nullptr;
                if (sourceClass && sourceClass->name &&
                    std::strcmp(sourceClass->name, "String") == 0) {
                    const std::string name = reinterpret_cast<MonoString*>(source)->str();
                    const std::int32_t converted = name == "Hand" ? 1 : name == "Deck" ? 2 : 0;
                    if (converted != 0) {
                        auto setItem = BNM::Class(properties).GetMethod("set_Item", 2).cast<void>();
                        auto* key = BNM::CreateMonoString("Source");
                        auto* boxed = objectBox(int32Class, const_cast<std::int32_t*>(&converted));
                        if (setItem.IsValid() && key && boxed) {
                            setItem[properties](key, boxed);
                            LOG_INFO("CustomComponents/CustomFieldBridge/QUERY: preconverted Source=%s to %d instance=%p",
                                     name.c_str(), int(converted), instance);
                        } else {
                            LOG_ERR("CustomComponents/CustomFieldBridge/QUERY: Source preconversion unavailable instance=%p", instance);
                        }
                    }
                }
            }
        }
        // GrantAbility receives properties after DiscardedFromQuery normalization.
        // Its delegate invokes the unhooked stock setter exactly once.
        if (delegatedSetObjectProperties)
            delegatedSetObjectProperties(self, instance, properties, method);
        else
            originalSetObjectProperties(self, instance, properties, method);
    }

    inline bool ReadSerializedField(Obj* properties, const Schema& schema, const ResolvedField& field, CapturedValue& out) {
        Obj* boxed = DictionaryValue(properties, field.name.c_str());
        if (!boxed)
            return false;

        out.present = true;
        out.kind = field.kind;
        auto* klass = BNM::Class(boxed).GetClass();
        const char* typeName = klass && klass->name ? klass->name : "";

        if (field.kind == FieldKind::String) {
            out.value.stringValue = ObjectToString(boxed);
            return out.value.stringValue != nullptr;
        }

        if (objectUnbox) {
            if (void* raw = objectUnbox(boxed)) {
                if (field.kind == FieldKind::Bool && std::strcmp(typeName, "Boolean") == 0) {
                    out.value.boolValue = *static_cast<bool*>(raw);
                    return true;
                }
                if (field.kind == FieldKind::Int32) {
                    if (std::strcmp(typeName, "Int32") == 0) {
                        out.value.intValue = *static_cast<std::int32_t*>(raw);
                        return true;
                    }
                    if (std::strcmp(typeName, "Int64") == 0) {
                        out.value.intValue = static_cast<std::int32_t>(*static_cast<std::int64_t*>(raw));
                        return true;
                    }
                }
                if (field.kind == FieldKind::Float) {
                    if (std::strcmp(typeName, "Single") == 0) {
                        out.value.floatValue = *static_cast<float*>(raw);
                        return true;
                    }
                    if (std::strcmp(typeName, "Double") == 0) {
                        out.value.floatValue = static_cast<float>(*static_cast<double*>(raw));
                        return true;
                    }
                }
            }
        }

        MonoString* text = ObjectToString(boxed);
        if (!text)
            return false;
        const std::string value = text->str();
        switch (field.kind) {
            case FieldKind::Bool:
                if (value == "True" || value == "true" || value == "1") {
                    out.value.boolValue = true;
                    return true;
                }
                if (value == "False" || value == "false" || value == "0") {
                    out.value.boolValue = false;
                    return true;
                }
                return false;
            case FieldKind::Int32: {
                // Only DiscardedFromQuery.Source supports these names.
                // Never register them globally for System.Int32.
                if (schema.namespaze == "PvZCards.Engine.Queries" &&
                    schema.typeName == "DiscardedFromQuery" && field.name == "Source") {
                    if (value == "Hand") { out.value.intValue = 1; return true; }
                    if (value == "Deck") { out.value.intValue = 2; return true; }
                }
                char* end = nullptr;
                const long parsed = std::strtol(value.c_str(), &end, 10);
                if (!end || *end != '\0')
                    return false;
                out.value.intValue = static_cast<std::int32_t>(parsed);
                return true;
            }
            case FieldKind::Float: {
                char* end = nullptr;
                const float parsed = std::strtof(value.c_str(), &end);
                if (!end || *end != '\0')
                    return false;
                out.value.floatValue = parsed;
                return true;
            }
            case FieldKind::String:
                return false;
        }
        return false;
    }

    inline void ApplySerializedFields(Obj*, Obj*, const char*);

    static void RefDeserializeIntoHook(
        void* self,
        void* objectToPopulate,
        void* serializedObject,
        const void* method
    ) {
        auto* object = static_cast<Obj*>(objectToPopulate);
        Schema* schema = FindSchema(object);
        if (!schema) {
            originalRefDeserializeInto(self, objectToPopulate, serializedObject, method);
            return;
        }

        std::vector<CapturedValue> captured(schema->fields.size());
        Obj* properties = nullptr;
        if (serializedObject && convertPropertiesToDictionary.IsValid()) {
            properties = convertPropertiesToDictionary[static_cast<Obj*>(nullptr)](
                static_cast<Obj*>(serializedObject)
            );
        }
        if (properties) {
            for (std::size_t i = 0; i < schema->fields.size(); ++i)
                ReadSerializedField(properties, *schema, schema->fields[i], captured[i]);
        }

        originalRefDeserializeInto(self, objectToPopulate, serializedObject, method);

        for (std::size_t i = 0; i < schema->fields.size(); ++i) {
            if (captured[i].present)
                WriteValue(object, schema->fields[i], captured[i]);
        }

        ApplySerializedFields(object, static_cast<Obj*>(serializedObject), "existing");
        LOG_INFO(
            "CustomComponents/CustomFieldBridge: deserialized %s.%s customFields=%zu",
            schema->namespaze.c_str(),
            schema->typeName.c_str(),
            schema->fields.size()
        );
    }


    // New-object deserialization is used for nested EntityQuery instances.
    // The existing two-argument hook covers populate-existing only.
    using RefDeserializeNewFn = Obj* (*)(void*, Obj*, const void*);
    inline RefDeserializeNewFn originalRefDeserializeNew = nullptr;

    inline void ApplySerializedFields(Obj* object, Obj* serializedObject, const char* phase) {
        Schema* schema = FindSchema(object);
        if (!schema || !serializedObject || !convertPropertiesToDictionary.IsValid()) return;
        Obj* properties = convertPropertiesToDictionary[static_cast<Obj*>(nullptr)](serializedObject);
        if (!properties) return;
        // Both the raw property map and wrapped $data forms are supported.
        Obj* data = DictionaryValue(properties, "$data");
        Obj* dataProperties = data ? convertPropertiesToDictionary[static_cast<Obj*>(nullptr)](data) : nullptr;
        std::size_t copied = 0;
        for (const auto& field : schema->fields) {
            CapturedValue captured;
            if ((dataProperties && ReadSerializedField(dataProperties, *schema, field, captured)) ||
                ReadSerializedField(properties, *schema, field, captured)) {
                WriteValue(object, field, captured);
                ++copied;
                if (schema->typeName == "DiscardedFromQuery" && field.name == "Source")
                    LOG_INFO("CustomComponents/CustomFieldBridge/QUERY: %s Source=%d object=%p",
                             phase, captured.value.intValue, object);
            }
        }
        if (schema->typeName == "DiscardedFromQuery")
            LOG_INFO("CustomComponents/CustomFieldBridge/QUERY: %s captured=%zu object=%p", phase, copied, object);
    }

    static Obj* RefDeserializeNewHook(void* self, Obj* serializedObject, const void* method) {
        Obj* result = originalRefDeserializeNew(self, serializedObject, method);
        // The reference deserializer is already hooked by this bridge. Trace
        // the return object here instead of layering a second native hook.
        static unsigned referenceTraceCount = 0;
        if (result && referenceTraceCount < 120) {
            auto* klass = BNM::Class(result).GetClass();
            if (klass && klass->name && std::strcmp(klass->name, "DiscardedFromQuery") == 0) {
                ++referenceTraceCount;
                auto sourceField = BNM::Class(result).GetField("Source").cast<std::int32_t>();
                LOG_INFO("CustomComponents/DiscardedFromQuery/RVA: ReferenceTypeDeserializer.Deserialize(object) returned query=%p source=%d serialized=%p (expected RVA=0x2221EE8)",
                    result, sourceField.IsValid() ? (int)sourceField[result].Get() : -999, serializedObject);
            }
        }
        ApplySerializedFields(result, serializedObject, "new");
        return result;
    }
    inline bool ResolveSchemas() {
        for (auto& schema : schemas) {
            auto klass = BNM::Class(schema.namespaze.c_str(), schema.typeName.c_str());
            if (!klass.IsValid()) {
                LOG_ERR(
                    "CustomComponents/CustomFieldBridge: class missing %s.%s",
                    schema.namespaze.c_str(), schema.typeName.c_str()
                );
                return false;
            }
            schema.klass = klass.GetClass();
            for (auto& field : schema.fields) {
                auto resolved = klass.GetField(field.name.c_str());
                if (!resolved.IsValid()) {
                    LOG_ERR(
                        "CustomComponents/CustomFieldBridge: field missing %s.%s.%s",
                        schema.namespaze.c_str(), schema.typeName.c_str(), field.name.c_str()
                    );
                    return false;
                }
                field.offset = static_cast<std::uintptr_t>(resolved.GetOffset());
                if (!field.offset) {
                    LOG_ERR(
                        "CustomComponents/CustomFieldBridge: invalid offset %s.%s.%s",
                        schema.namespaze.c_str(), schema.typeName.c_str(), field.name.c_str()
                    );
                    return false;
                }
            }
        }
        return true;
    }


    template<typename T>
    using MonoList = BNM::Structures::Mono::List<T>;

    inline BNM::Field<MonoList<Obj*>*> entityComponents;

    using EntityDeepCopyFn = void (*)(void*, Obj*, Obj*, const void*);
    inline EntityDeepCopyFn originalEntityDeepCopy = nullptr;

    inline Obj* FindExactSchemaComponent(Obj* entity, const Schema& schema) {
        if (!entity || !entityComponents.IsValid() || !schema.klass)
            return nullptr;

        auto* components = entityComponents[entity].Get();
        if (!components || !components->items)
            return nullptr;

        for (int i = 0; i < components->size; ++i) {
            Obj* component = components->items->m_Items[i];
            if (!component)
                continue;

            if (BNM::Class(component).GetClass() == schema.klass)
                return component;
        }

        return nullptr;
    }

    static void EntityDeepCopyHook(
        void* self,
        Obj* source,
        Obj* instances,
        const void* method
    ) {
        auto* target = static_cast<Obj*>(self);

        // Capture registered custom fields from the source BEFORE stock copying.
        // Entity.DeepCopyFrom creates fresh destination components itself, and
        // their stock DeepCopyFrom implementation does not know about BNM-added
        // fields.
        struct PendingSchemaCopy {
            Schema* schema = nullptr;
            std::vector<CapturedValue> values;
        };

        std::vector<PendingSchemaCopy> pending;
        if (source) {
            for (auto& schema : schemas) {
                Obj* sourceComponent = FindExactSchemaComponent(source, schema);
                if (!sourceComponent)
                    continue;

                PendingSchemaCopy entry;
                entry.schema = &schema;
                entry.values.reserve(schema.fields.size());

                for (const auto& field : schema.fields)
                    entry.values.push_back(ReadValue(sourceComponent, field));

                pending.push_back(std::move(entry));
            }
        }

        originalEntityDeepCopy(self, source, instances, method);

        // Stock has now created/copied the destination Components list.
        // Repair ONLY exact registered custom component types.
        for (auto& entry : pending) {
            if (!entry.schema)
                continue;

            Obj* targetComponent = FindExactSchemaComponent(target, *entry.schema);
            if (!targetComponent)
                continue;

            std::size_t copied = 0;
            for (std::size_t i = 0; i < entry.schema->fields.size(); ++i) {
                if (!entry.values[i].present)
                    continue;

                WriteValue(
                    targetComponent,
                    entry.schema->fields[i],
                    entry.values[i]
                );
                ++copied;
            }

            LOG_INFO(
                "CustomComponents/CustomFieldBridge: Entity.DeepCopyFrom preserved %s.%s sourceEntity=%p targetEntity=%p targetComponent=%p customFields=%zu",
                entry.schema->namespaze.c_str(),
                entry.schema->typeName.c_str(),
                source,
                target,
                targetComponent,
                copied
            );
        }
    }

    inline bool Initialize() {
        if (initialized)
            return true;
        if (schemas.empty()) {
            LOG_ERR("CustomComponents/CustomFieldBridge: no schemas registered");
            return false;
        }
        if (!ResolveSchemas())
            return false;

        auto referenceDeserializer = BNM::Class(
            "PvZCards.Core.Serialization",
            "ReferenceTypeDeserializer"
        );
        if (!referenceDeserializer.IsValid())
            return false;

        auto deserialize = referenceDeserializer.GetMethod("Deserialize", {
            BNM::Defaults::Get<Obj*>(),
            BNM::Defaults::Get<Obj*>()
        });
        convertPropertiesToDictionary =
            referenceDeserializer.GetMethod("ConvertPropertiesToDictionary", 1).cast<Obj*>();
        if (!deserialize.IsValid() || !convertPropertiesToDictionary.IsValid())
            return false;

        if (void* il2cpp = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD)) {
            objectUnbox = reinterpret_cast<decltype(objectUnbox)>(
                dlsym(il2cpp, "il2cpp_object_unbox")
            );
            objectBox = reinterpret_cast<decltype(objectBox)>(
                dlsym(il2cpp, "il2cpp_value_box")
            );
        }
        if (!objectUnbox) {
            LOG_ERR("CustomComponents/CustomFieldBridge: il2cpp_object_unbox unavailable");
            return false;
        }

        // The stock setter consumes the dictionary before either Deserialize
        // overload returns; post-deserialize field repair is too late for strings.
        int32Class = BNM::Class("System", "Int32").GetClass();
        auto setProperties = referenceDeserializer.GetMethod("SetObjectProperties", 2);
        if (!objectBox || !int32Class || !setProperties.IsValid()) {
            LOG_ERR("CustomComponents/CustomFieldBridge: scoped Source preconversion metadata unavailable");
            return false;
        }
        BNM::BasicHook(setProperties, SetObjectPropertiesHook, originalSetObjectProperties);
        if (!originalSetObjectProperties) {
            LOG_ERR("CustomComponents/CustomFieldBridge: SetObjectProperties hook failed");
            return false;
        }

        auto deserializeNew = referenceDeserializer.GetMethod("Deserialize", 1);
        if (!deserializeNew.IsValid()) {
            LOG_ERR("CustomComponents/CustomFieldBridge: new-object Deserialize overload missing");
            return false;
        }
        BNM::BasicHook(deserializeNew, RefDeserializeNewHook, originalRefDeserializeNew);
        if (!originalRefDeserializeNew) {
            LOG_ERR("CustomComponents/CustomFieldBridge: new-object Deserialize hook failed");
            return false;
        }

        BNM::BasicHook(deserialize, RefDeserializeIntoHook, originalRefDeserializeInto);
        if (!originalRefDeserializeInto) {
            LOG_ERR("CustomComponents/CustomFieldBridge: Deserialize hook failed");
            return false;
        }

        auto entityClass = BNM::Class("PvZCards.Engine", "Entity");
        if (!entityClass.IsValid()) {
            LOG_ERR("CustomComponents/CustomFieldBridge: Entity class missing");
            return false;
        }

        entityComponents = entityClass.GetField("Components");
        auto entityDeepCopy = entityClass.GetMethod("DeepCopyFrom", 2);

        if (!entityComponents.IsValid() || !entityDeepCopy.IsValid()) {
            LOG_ERR(
                "CustomComponents/CustomFieldBridge: Entity copy metadata missing components=%d deepCopy=%d",
                entityComponents.IsValid() ? 1 : 0,
                entityDeepCopy.IsValid() ? 1 : 0
            );
            return false;
        }

        BNM::BasicHook(
            entityDeepCopy,
            EntityDeepCopyHook,
            originalEntityDeepCopy
        );
        if (!originalEntityDeepCopy) {
            LOG_ERR(
                "CustomComponents/CustomFieldBridge: Entity.DeepCopyFrom hook failed"
            );
            return false;
        }

        initialized = true;
        LOG_INFO(
            "CustomComponents/CustomFieldBridge: ready schemas=%zu deserialize+Entity.DeepCopyFrom bridge address=%p",
            schemas.size(),
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(entityDeepCopy.GetOffset())
            )
        );
        return true;
    }

} // namespace CustomComponents::Systems::CustomFieldBridgeSystem
