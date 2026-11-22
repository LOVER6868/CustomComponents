#pragma once
namespace CustomComponents::Core {
enum class EnqueueResult { Added, Suppressed, RecycleUnavailable };
template<class Context,class Filters,class Recycle,class Decorate,class Original>
EnqueueResult Enqueue(const Context& context,const Filters& filters,Recycle recycle,Decorate decorate,Original original) {
    bool suppress=false;
    for(auto filter:filters) suppress=filter(context)||suppress;
    if(suppress && recycle(context)) return EnqueueResult::Suppressed;
    decorate(context);
    original(context);
    return suppress?EnqueueResult::RecycleUnavailable:EnqueueResult::Added;
}
}
