#include "pick3/PickedFromThreeSelection.hpp"
#include <cassert>
int main() {
 using namespace CustomComponents::Pick3::Picked;
 int id = -1;
 ResetMatch();
 assert(Store(28,255,{{-1,647}}));
 assert(!TryGetSingleBoundDatabaseEntity(28,255,id));
 assert(BindCreated(28,647,153));
 assert(TryGetSingleBoundDatabaseEntity(28,255,id) && id==153);
 assert(!TryGetSingleBoundDatabaseEntity(28,254,id));
 assert(!TryGetSingleBoundDatabaseEntity(27,255,id));
 assert(Store(28,256,{{26,647}})); // regular entity selection
 assert(!TryGetSingleBoundDatabaseEntity(28,256,id));
 assert(Store(28,257,{{-1,647},{-1,425}})); // ambiguity
 assert(BindCreated(28,647,154));
 assert(!TryGetSingleBoundDatabaseEntity(28,257,id));
 ResetMatch();
 assert(!TryGetSingleBoundDatabaseEntity(28,257,id));
}
