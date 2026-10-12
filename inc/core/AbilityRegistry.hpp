#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace CustomComponents {
    enum class AbilityStorage {
        Numeric,
        Grantable
    };

    struct AbilityDescriptor {
        static constexpr int NoSpecialAbilityType = -1;

        int id{};
        const char* name{};
        AbilityStorage storage{AbilityStorage::Grantable};
        void* (*component)(void* entity, bool create){};
        void* (*numericValue)(void* entity, bool create){};
        int specialAbilityType{NoSpecialAbilityType};
    };

    using AbilityAdapter = AbilityDescriptor;

    class AbilityRegistry {
        inline static std::array<AbilityDescriptor, 320> entries{};
        inline static std::size_t count = 0;

    public:
        static bool Register(AbilityDescriptor entry) {
            const bool hasStorage = entry.storage == AbilityStorage::Numeric
                ? entry.numericValue != nullptr
                : entry.component != nullptr;
            if (!entry.name || !hasStorage || entry.id < 19 || count == entries.size() ||
                Find(entry.id) || Find(entry.name)) return false;
            entries[count++] = entry;
            return true;
        }

        static const AbilityDescriptor* Find(int id) {
            for (std::size_t i = 0; i < count; ++i)
                if (entries[i].id == id) return &entries[i];
            return nullptr;
        }

        static const AbilityDescriptor* Find(std::string_view name) {
            for (std::size_t i = 0; i < count; ++i)
                if (name == entries[i].name) return &entries[i];
            return nullptr;
        }

        template<class F> static void ForEach(F callback) {
            for (std::size_t i = 0; i < count; ++i) callback(entries[i]);
        }

        static std::size_t Count() { return count; }
    };
}
