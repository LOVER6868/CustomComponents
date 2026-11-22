#pragma once
#include <algorithm>
#include "level/LevelVisualStyle.hpp"
namespace CustomComponents::Level::Pulse {
inline constexpr double Duration=VisualStyle::PulseDuration;
inline constexpr float Amplitude=VisualStyle::PulseAmplitude;
inline float Scale(double elapsed){float t=float(std::clamp(elapsed/Duration,0.0,1.0));float triangle=t<.5f?t*2:(1-t)*2;return 1+Amplitude*(triangle*triangle*(3-2*triangle));}
}
