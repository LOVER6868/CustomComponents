#include <cassert>
#include "pick3/SelectionSubmissionLedger.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
namespace RT=CustomComponents::Pick3::TransactionRuntime;
namespace TX=CustomComponents::Pick3::Transaction;
namespace P=CustomComponents::Pick3::Picked;
int main() {
    RT::ResetSubmissions();TX::Reset();
    // Service stack has exited; replica queue still retrieves its exact choice.
    {RT::Submission s{46,237,10,{31},{{31,665,-1}}};assert(RT::StageSubmission(1,s));}
    auto s=RT::PendingFor(1,46,237);assert(s.sequence==10 && s.cards[0].entityId==31);
    assert(!RT::PendingFor(2,46,237).sequence); // prediction cannot borrow it
    assert(!RT::PendingFor(1,46,238).sequence);
    assert(!RT::PendingFor(1,47,237).sequence);
    // A duplicate receive preserves the first generation for ClaimEvent.
    assert(RT::StageSubmission(1,{46,237,11,{31},{{31,665,-1}}}));
    assert(RT::PendingFor(1,46,237).sequence==10);
    assert(!RT::StageSubmission(1,{46,237,12,{32},{{32,665,-1}}}));
    assert(RT::StageSubmission(2,{46,237,20,{32},{{32,665,-1}}}));
    P::SelectionSnapshot snap;snap.sequence=s.sequence;snap.selection={s.effect,s.group,s.cards};
    auto a=TX::Make(1,9,snap);assert(a && TX::ClaimEvent(a));
    assert(!TX::ClaimEvent(TX::Make(1,9,snap)));
    snap.sequence=20;snap.selection.cards={{32,665,-1}};
    assert(TX::ClaimEvent(TX::Make(2,9,snap)));
    // Database GUID and legal recipient remain separate across deferred drain.
    assert(RT::StageSubmission(1,{59,393,30,{8},{{-1,716,-1}}}));
    auto db=RT::PendingFor(1,59,393);
    assert(db.targets[0]==8 && db.cards[0].entityId==-1 && db.cards[0].cardGuid==716);
    assert(!RT::StageSubmission(0,s));
    RT::ResetSubmissions();assert(!RT::PendingFor(1,46,237).sequence);
}
