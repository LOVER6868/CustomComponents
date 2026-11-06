#pragma once
#include <string>
namespace CustomComponents::Presentation::HeraldPolicy {
inline bool CustomDiscard(int source,bool inDiscard){return inDiscard && (source==1 || source==2);}
inline std::string Origin(bool discarded,const std::string& existing){return discarded?"discarded":existing;}
}
