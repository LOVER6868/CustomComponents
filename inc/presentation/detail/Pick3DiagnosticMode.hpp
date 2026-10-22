#pragma once
// Release build: retain no-op compatibility for the old diagnostic calls.
namespace CustomComponents::Presentation::Pick3DiagnosticMode {
 inline void Mark(const char*,int,int) {}
 inline void Complete(const char*,int,int) {}
 inline void Startup() {}
 inline int ReadMode() {return 0;}
 inline void ResetRecovery() {}
}
