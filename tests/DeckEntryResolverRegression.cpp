// Host: clang++ -std=c++20 -O2 -I../inc DeckEntryResolverRegression.cpp -o deck_resolver_test
#include "queries/DeckEntryResolver.hpp"
#include <cassert>
#include <iostream>
using namespace CustomComponents::Queries::DeckEntries;
int main() {
    const std::vector<Selection> pool = {
        {10,0,0,673,-1}, {10,0,1,673,-1}, {10,0,2,317,44},
        {20,0,0,673,51}, {10,1,0,673,-1}};
    assert(CountCopies(pool,10,673)==3);
    assert(CountCopies(pool,20,673)==1);
    auto own = ForOwner(pool,10);
    assert(own.size()==4 && own[0].entityId==-1);
    assert(Validate(pool,pool[1]));
    auto v = ResolveSaved(pool, pool[1]);
    assert(v.status == ResolveStatus::VirtualEntry && v.entityId == -1);
    auto live = ResolveSaved(pool, pool[2]);
    assert(live.status == ResolveStatus::FoundEntity && live.entityId == 44);
    assert(ResolveEntity(pool, 10, 44).status == ResolveStatus::FoundEntity);
    assert(ResolveEntity(pool, 20, 44).status == ResolveStatus::AmbiguousEntity);
    assert(ResolveEntity(pool, 10, 673).status == ResolveStatus::InvalidSelection);
    auto duplicate = pool;
    duplicate.push_back({10,0,3,317,44});
    assert(ResolveEntity(duplicate, 10, 44).status == ResolveStatus::AmbiguousEntity);
    assert(ResolveSaved(duplicate, pool[2]).status == ResolveStatus::AmbiguousEntity);

    auto changed=pool;
    changed[1].guid=317;
    assert(ResolveSaved(changed,pool[1]).status == ResolveStatus::EntryChanged);
    assert(!Validate(changed,pool[1]));
    changed=pool;
    changed[1].entityId=62;
    assert(!Validate(changed,pool[1]));
    changed=pool;
    changed.erase(changed.begin());
    // Entries shifted; the old physical index is no longer trustworthy.
    assert(!Validate(changed,pool[0]));
    assert(pool.size()==5 && pool[0].entityId==-1);
    std::cout << "PASS: virtual copies, ownership, identity checks, read-only behavior\\n";
}
