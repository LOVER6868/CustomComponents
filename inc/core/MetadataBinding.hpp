#pragma once

namespace CustomComponents::Core::Metadata {
// BNM's operator[] mutates the handle's current instance. Bind a copy so
// simulation workers and Unity presentation never overwrite one another's
// cached field or method receiver.
template<class Handle,class Instance>
inline Handle Bind(Handle handle,Instance* instance) {
    handle[instance];
    return handle;
}
}
