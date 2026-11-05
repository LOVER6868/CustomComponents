#include <cassert>
#include "effects/detail/PhysicalDeckSelection.hpp"
int main(){
 using namespace CustomComponents::DeckSelection;
 std::vector<Entry> deck{{31,665,false},{32,665,false},{33,700,false},{-1,665,false}};
 assert(FirstExactEntry(deck,{{32,665}})==1);
 assert(FirstExactEntry(deck,{{99,665}})==-1);
 assert(FirstExactEntry(deck,{{31,700}})==-1);
 assert(FirstExactEntry(deck,{{33,700},{31,665}})==0);
 deck[0].reserved=true;assert(FirstExactEntry(deck,{{31,665},{32,665}})==1);
 assert(FirstExactEntry(deck,{})==-1);
 assert(FirstExactEntry(deck,{{-1,665}})==-1);
}
