#include "diagnostics/PvpInsertionSafeLookup.hpp"
#include <cassert>
using CustomComponents::Diagnostics::PvpInsertionSafeLookup::FindUnique;
struct Entity { int id; };
int main() {
    Entity a{1}, b{6}, c{245}, duplicate{6}, unknown{0};
    Entity* collection[]={&a,nullptr,&b,&c,&unknown};
    auto readId=[](Entity* p){return p->id;};
    assert(FindUnique(collection,5,1,readId)==&a);
    assert(FindUnique(collection,5,6,readId)==&b);
    assert(FindUnique(collection,5,245,readId)==&c);
    assert(FindUnique(collection,5,7,readId)==nullptr);
    assert(FindUnique(collection,5,-1,readId)==nullptr);
    assert(FindUnique(collection,5,0,readId)==nullptr);
    assert(FindUnique(collection,0,1,readId)==nullptr);
    assert(FindUnique<Entity>(nullptr,5,1,readId)==nullptr);
    Entity* ambiguous[]={&a,&b,&duplicate};
    assert(FindUnique(ambiguous,3,6,readId)==nullptr);
    // Fails closed instead of invoking a managed lookup or resolving a
    // duplicate ID nondeterministically.
    assert(FindUnique(ambiguous,3,1,readId)==&a);
    return 0;
}
