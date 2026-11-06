#pragma once
#include "pick3/SelectionTransaction.hpp"

namespace CustomComponents::Pick3::TransactionRuntime {
struct Submission {
    int effect=-1,group=-1;
    std::uint64_t sequence=0;
    std::vector<int> targets;
    std::vector<Picked::SelectedCard> cards;
};
// Decoded transport intent, not acceptance. Queue processing may happen after
// the service stack unwinds. Never recover intent from shared/latest history.
struct PendingSubmission { Transaction::Model model; Submission value; bool completed=false; };
inline std::mutex pendingSubmissionLock;
inline std::vector<PendingSubmission> pendingSubmissions;
inline bool StageSubmission(Transaction::Model model,const Submission& value) {
    if(!model || value.effect<=0 || value.group<0 || !value.sequence ||
       value.targets.empty() || value.targets.size()>3 || value.cards.empty() || value.cards.size()>3) return false;
    std::lock_guard<std::mutex> guard(pendingSubmissionLock);
    for(auto& item:pendingSubmissions) if(item.model==model && item.value.effect==value.effect && item.value.group==value.group) {
        if(item.completed && item.value.sequence!=value.sequence) {
            item.value=value;item.completed=false;return true;
        }
        // Duplicate delivery keeps the same generation. Conflicting intent
        // cannot replace an outstanding choice for this model/queue identity.
        if(item.value.targets!=value.targets || item.value.cards.size()!=value.cards.size()) return false;
        for(std::size_t i=0;i<value.cards.size();++i)
            if(item.value.cards[i].entityId!=value.cards[i].entityId || item.value.cards[i].cardGuid!=value.cards[i].cardGuid) return false;
        return true;
    }
    if(pendingSubmissions.size()>=Transaction::capacity) {
        auto retired=std::find_if(pendingSubmissions.begin(),pendingSubmissions.end(),[](const auto& item){return item.completed;});
        if(retired==pendingSubmissions.end()) return false;
        pendingSubmissions.erase(retired);
    }
    pendingSubmissions.push_back({model,value});return true;
}
inline Submission PendingFor(Transaction::Model model,int effect,int group) {
    std::lock_guard<std::mutex> guard(pendingSubmissionLock);
    for(const auto& item:pendingSubmissions)
        if(item.model==model && item.value.effect==effect && item.value.group==group) return item.value;
    return {};
}
inline void ResetSubmissions() {
    std::lock_guard<std::mutex> guard(pendingSubmissionLock);
    pendingSubmissions.clear();
}
inline void CompleteSubmission(Transaction::Model model,int effect,int group,std::uint64_t sequence) {
    std::lock_guard<std::mutex> guard(pendingSubmissionLock);
    for(auto& item:pendingSubmissions) if(item.model==model && item.value.effect==effect &&
        item.value.group==group && item.value.sequence==sequence) item.completed=true;
}
inline void RetireSubmissions(Transaction::Model model) {
    std::lock_guard<std::mutex> guard(pendingSubmissionLock);
    pendingSubmissions.erase(std::remove_if(pendingSubmissions.begin(),pendingSubmissions.end(),
        [&](const auto& item){return item.model==model;}),pendingSubmissions.end());
}
}
