// Host-only test of observational execution scoping; no IL2CPP runtime needed.
#include <cassert>
#include "pick3/Patch2ProvenanceObservation.hpp"
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
using namespace CustomComponents::Pick3;
int main() {
    Picked::ResetMatch();
    assert(Picked::Store(61, 197, {{24, 710, -1}}));
    {
        Provenance::ScopedQueueExecution one(61, 197, 4, 5);
        assert(Provenance::active.effect == 61 && Provenance::active.group == 197);
        assert(Provenance::active.snapshot == 1);
        {
            Provenance::ScopedQueueExecution other(999, 101, 4, 5);
            assert(Provenance::active.effect == 999 && Provenance::active.snapshot == 0);
        }
        assert(Provenance::active.snapshot == 1);
        Provenance::Query("GUID", 61, 710, -1, true);
    }
    assert(Provenance::active.depth == 0 && Provenance::active.snapshot == 0);
    // Two historical generations on the same exact effect/group: fail closed.
    Picked::ObserveCompletion(61, 197, true);
    assert(Picked::Store(61, 197, {{24, 710, -1}}));
    {
        Provenance::ScopedQueueExecution ambiguous(61, 197, 4, 5);
        assert(Provenance::active.snapshot == 0);
        Provenance::Query("GUID", 61, 710, -1, true);
    }
    Picked::ResetMatch();
    return 0;
}
