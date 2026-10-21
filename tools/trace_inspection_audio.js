// Read-only Frida tracer for PvZ Heroes ARM64 libil2cpp.so.
// Usage: frida -U -n com.ea.gp.pvzheroes -l tools/trace_inspection_audio.js
// Requires a matching, authorized device/debugging setup and exact game binary.
// RVA based on the user's current libil2cpp dump. No calls are intercepted/replaced.
'use strict';
const name = 'libil2cpp.so';
const rva = 0x1C9B320; // SoundManager.GenericSoundEvent(string, GameObject)
const base = Process.getModuleByName(name).base;
const target = base.add(rva);
const counts = {};
function readIl2CppString(ptr) {
  if (ptr.isNull()) return '<null>';
  try {
    const n = ptr.add(0x10).readS32();
    if (n < 0 || n > 256) return '<bad-length:' + n + '>';
    return ptr.add(0x14).readUtf16String(n);
  } catch (e) { return '<unreadable>'; }
}
console.log('[Pick3 SOUND TRACE] GenericSoundEvent at ' + target);
Interceptor.attach(target, {
  onEnter(args) {
    const event = readIl2CppString(args[1]);
    // Limit to inspection and button UI. Unknown events can be discovered
    // by widening the filter temporarily. Keep game behavior unchanged.
    if (!/INSPECTION|BUTTON|AUTOGUI|DISCARD/i.test(event)) return;
    counts[event] = (counts[event] || 0) + 1;
    if (counts[event] <= 20)
      console.log('[Pick3 SOUND TRACE] ' + event + ' #' + counts[event]);
  }
});
