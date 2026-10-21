#pragma once
#include <cstddef>
#include <cstdint>
namespace CustomComponents::Presentation::Pick3RewardArt {
    // Only sprites used by the active, self-owned static UI pipeline.
    enum Kind { DescriptionPlate, CostSun, CostBrain, AttackLeaf, HealthHeart,
                InspectionGrass, InspectionReferenceRim, InlineTooltipPlate, InspectionRimLeft, InspectionRimRight, ChooseButton, Count };
    struct Blob { const std::uint8_t* bytes; std::size_t size; int width, height; };
    Blob Get(Kind kind);
}
