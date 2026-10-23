#pragma once
#include <mutex>
#include <cstdint>
namespace CustomComponents::SelectionCompletion {
enum class Result { Idle, Pending, Success, Failure, Uncertain };
// A callback ticket belongs to one attempt, even when effect IDs are recycled.
// Callback threads only publish a result; Unity/IL2CPP cleanup runs on UI Tick.
class CompletionState {
public:
    std::uint64_t Begin() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (result_ != Result::Idle) return 0;
        ++ticket_; result_=Result::Pending; reason_=0; return ticket_;
    }
    bool Complete(std::uint64_t ticket, Result result, int reason=0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ticket != ticket_ || result_ != Result::Pending ||
            (result != Result::Success && result != Result::Failure && result != Result::Uncertain)) return false;
        result_=result; reason_=reason; return true;
    }
    void Invalidate() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++ticket_; result_=Result::Idle; reason_=0;
    }
    Result Status() const { std::lock_guard<std::mutex> lock(mutex_); return result_; }
    int Reason() const { std::lock_guard<std::mutex> lock(mutex_); return reason_; }
private:
    mutable std::mutex mutex_;
    std::uint64_t ticket_=0;
    Result result_=Result::Idle;
    int reason_=0;
};
}
