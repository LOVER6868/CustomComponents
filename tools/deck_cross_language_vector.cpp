#include "queries/DeckEntryWireIdentity.hpp"
#include <iostream>
int main(){
 using namespace CustomComponents::Queries;
 std::vector<DeckEntries::Selection> entries={{1,0,0,709,-1},{1,0,1,709,55},{1,0,2,334,56},{2,0,0,715,70}};
 DeckWire::Intent i;
 if(!DeckWire::Capture(entries,entries[0],i))return 1;
 for(const auto w:DeckWire::Encode(i)) std::cout << w << ' ';
 std::cout << '\n';
}
