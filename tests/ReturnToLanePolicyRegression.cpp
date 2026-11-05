#include <cassert>
#include <initializer_list>
#include "zones/ReturnToLanePolicy.hpp"
namespace P=CustomComponents::Systems::ReturnToLanePolicy;
int main() {
    assert(P::Classify(true,false)==P::CardKind::Fighter);
    assert(P::Classify(false,true)==P::CardKind::Environment);
    assert(P::Classify(false,false)==P::CardKind::Unsupported); // tricks
    assert(P::Classify(true,true)==P::CardKind::Unsupported); // malformed data
    for(int count=-1;count<=4;++count) {
        assert(P::HasSlot(P::CardKind::Environment,count,false,false,false));
        assert(!P::HasSlot(P::CardKind::Unsupported,count,true,true,true));
        for(bool plant:{false,true}) for(bool incoming:{false,true}) for(bool resident:{false,true}) {
            const bool expected=count==0 || (count==1 && plant && (incoming || resident));
            assert(P::HasSlot(P::CardKind::Fighter,count,plant,incoming,resident)==expected);
        }
    }
}
