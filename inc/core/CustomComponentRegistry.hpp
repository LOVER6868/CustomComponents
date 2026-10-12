#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>

namespace CustomComponents {

    // Native registry for custom managed components. It intentionally does not
    // introduce a managed CustomComponent base class: every component can continue to
    // inherit directly from the vanilla PvZ component class it actually needs.
    struct ComponentDescriptor {
        const char* name{};
        const char* namespaze{};
        const char* assembly{};
        const char* baseName{};
        const char* baseNamespace{};
        const char* baseAssembly{};
        BNM::MonoType* monoType{};

    };

    class CustomComponentRegistry {
    public:
        static constexpr std::size_t MaxComponents = 164;

        static bool Register(ComponentDescriptor descriptor) {
            if (sCount >= MaxComponents) return false;
            if (FindByName(descriptor.namespaze, descriptor.name)) return false;

            BNM::Class klass(descriptor.namespaze, descriptor.name);
            if (!klass.IsValid()) return false;

            descriptor.monoType = klass.GetMonoType();
            if (!descriptor.monoType) return false;

            sComponents[sCount++] = descriptor;
            return true;
        }

        static ComponentDescriptor* FindByType(void* type) {
            if (!type) return nullptr;
            for (std::size_t i = 0; i < sCount; ++i) {
                if (static_cast<void*>(sComponents[i].monoType) == type)
                    return &sComponents[i];
            }
            return nullptr;
        }

        static ComponentDescriptor* FindByName(const char* namespaze, const char* name) {
            for (std::size_t i = 0; i < sCount; ++i) {
                if (std::strcmp(sComponents[i].namespaze, namespaze) == 0 &&
                    std::strcmp(sComponents[i].name, name) == 0)
                    return &sComponents[i];
            }
            return nullptr;
        }

        static ComponentDescriptor* FindSerializedType(const std::string& text) {
            for (std::size_t i = 0; i < sCount; ++i) {
                std::string prefix = std::string(sComponents[i].namespaze) + "." + sComponents[i].name + ",";
                if (text.rfind(prefix, 0) == 0)
                    return &sComponents[i];
            }
            return nullptr;
        }

        // Generic construction path shared by Charge and future custom components.
        // It preserves the exact behavior proven by Charge Test 4: allocate the
        // BNM class directly, then invoke the real vanilla base .ctor.
        static void* CreateInstance(ComponentDescriptor& component) {
            BNM::Class klass(component.namespaze, component.name);
            if (!klass.IsValid()) return nullptr;

            void* instance = klass.CreateNewInstance();
            if (!instance) return nullptr;

            BNM::Class base(component.baseNamespace, component.baseName);
            if (!base.IsValid()) return nullptr;

            auto ctor = base.GetMethod(".ctor", 0);
            if (!ctor.IsValid()) return nullptr;

            ctor.cast<void>()[instance]();

            return instance;
        }

        static std::size_t Count() { return sCount; }

    private:
        inline static std::array<ComponentDescriptor, MaxComponents> sComponents{};
        inline static std::size_t sCount = 0;
    };

} // namespace CustomComponents
