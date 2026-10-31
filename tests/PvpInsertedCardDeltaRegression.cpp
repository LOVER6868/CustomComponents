#include "diagnostics/PvpInsertionDelta.hpp"
#include <cassert>
#include <cstdint>
using namespace CustomComponents::Diagnostics::PvpInsertionDelta;
int main() {
    const void* a=reinterpret_cast<const void*>(0x100);
    const void* b=reinterpret_cast<const void*>(0x200);
    const void* c=reinterpret_cast<const void*>(0x300);
    const void* d=reinterpret_cast<const void*>(0x400);
    const void* e=reinterpret_cast<const void*>(0x500);
    const void* f=reinterpret_cast<const void*>(0x600);
    std::vector<Entry> baseline{{a,40,321},{b,40,322},{c,709,-1}};
    // Existing -1 entries must be preserved, not mistaken for new insertions;
    // insertion positions need not be at the end of the deck.
    std::vector<Entry> inserted{{d,500,-1},{a,40,321},{e,500,-1},
                                {b,40,322},{c,709,-1},{f,500,-1}};
    auto valid=Compare(baseline,inserted,500,3);
    assert(valid.valid && valid.newIndices.size()==3);
    assert(valid.newIndices[0]==0 && valid.newIndices[1]==2 && valid.newIndices[2]==5);
    assert(!Compare(baseline,inserted,500,4).valid);
    assert(!Compare(baseline,inserted,501,3).valid);
    auto modified=inserted; modified[1].entityId=-1;
    assert(!Compare(baseline,modified,500,3).valid);
    auto lost=inserted; lost[3].identity=a;
    assert(!Compare(baseline,lost,500,3).valid);
    auto duplicated=inserted;duplicated[2].identity=d;
    assert(!Compare(baseline,duplicated,500,3).valid);
    auto physicallyInserted=inserted; physicallyInserted[0].entityId=999;
    assert(!Compare(baseline,physicallyInserted,500,3).valid);
    auto invalidCount=Compare(baseline,baseline,500,3);
    assert(!invalidCount.valid);
    // Sow Magic Beans: two successive four-card insertion batches. Cards from
    // the first batch have been materialized before the next stock insertion.
    std::vector<Entry> deck{{a,40,321},{b,40,322}};
    const void* beans[8] = {
      reinterpret_cast<const void*>(0x700),reinterpret_cast<const void*>(0x701),
      reinterpret_cast<const void*>(0x702),reinterpret_cast<const void*>(0x703),
      reinterpret_cast<const void*>(0x704),reinterpret_cast<const void*>(0x705),
      reinterpret_cast<const void*>(0x706),reinterpret_cast<const void*>(0x707)
    };
    std::vector<Entry> four{{beans[0],500,-1},{a,40,321},{beans[1],500,-1},
                            {beans[2],500,-1},{b,40,322},{beans[3],500,-1}};
    assert(Compare(deck,four,500,4).valid);
    for (auto& entry:four) if (entry.guid==500) entry.entityId=400+int(entry.identity==beans[0]?0:entry.identity==beans[1]?1:entry.identity==beans[2]?2:3);
    std::vector<Entry> eight{{beans[4],500,-1},four[0],four[1],four[2],
                             {beans[5],500,-1},four[3],four[4],four[5],
                             {beans[6],500,-1},{beans[7],500,-1}};
    assert(Compare(four,eight,500,4).valid);
    std::uint64_t h=14695981039346656037ull;
    Mix32(h,500); Mix32(h,321); Mix32(h,322);
    assert(h!=14695981039346656037ull);
    return 0;
}
