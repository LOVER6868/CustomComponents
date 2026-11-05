#include <cassert>
#include <cstring>
#include <vector>
#include "triggers/ReturnToLaneMoveFilter.hpp"
struct Entity {int id; const char* component;};
using Model=std::vector<Entity>;
static Entity* Find(Model* model,int id) {
    for(auto& entity:*model) if(entity.id==id) return &entity;
    return nullptr;
}
static bool Has(Entity* entity,const char* name) {
    return entity && std::strcmp(entity->component,name)==0;
}
static bool Suppress(Model* model,int listener,int origin) {
    return CustomComponents::Systems::ReturnToLaneMoveFilter::ShouldSuppress(model,listener,origin,Find,Has);
}
int main() {
    Model live{{10,"MoveTrigger"},{11,"PlayTrigger"},{12,"ReturnedFromDiscardPileTrigger"},
        {20,"ReturnFromDiscardPileToLaneEffectDescriptor"},{21,"MoveCardToLanesEffectDescriptor"}};
    Model clone{{10,"MoveTrigger"},{20,"MoveCardToLanesEffectDescriptor"}};
    assert(Suppress(&live,10,20));
    assert(!Suppress(&live,10,21)); // ordinary movement still fires
    assert(!Suppress(&live,11,20)); // return keeps play event
    assert(!Suppress(&live,12,20)); // return keeps return event
    assert(!Suppress(&clone,10,20)); // identical IDs, different model
    assert(!Suppress(&live,10,99));
    assert(!Suppress(&live,99,20));
    assert(!Suppress(nullptr,10,20));
    assert(!Suppress(&live,-1,20));
}
