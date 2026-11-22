#pragma once
namespace CustomComponents::Core::PresentationInvalidation {
inline void (*invalidateFrames)()=nullptr;
inline void (*invalidateCard)(int)=nullptr;
inline void Card(int id){if(id>=0 && invalidateCard)invalidateCard(id);else if(invalidateFrames)invalidateFrames();}
inline void Frames(){if(invalidateFrames)invalidateFrames();}
}
