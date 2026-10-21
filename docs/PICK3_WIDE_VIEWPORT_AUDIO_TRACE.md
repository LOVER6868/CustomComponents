# PickFromThree large transparent viewport + inspection sound trace

- Native animation output uses a 512×512 square RenderTexture, shown in an *unmasked* square UGUI RawImage with default size 250% of card width. A matching 2.5× camera field-of-view change prevents the art itself from growing 2.5×. The inspection/class circle remains decorative and stats stay separate.
- This reduces card-image clipping by expanding the *actual camera view*, not only the UGUI quad. It **cannot guarantee** no clipping for particles or animations beyond a finite RenderTexture. Camera bounds still depend on a renderer cluster; this is **not** a complete stock attachment/position system.
- 512px render textures multiply pixel area by 4 compared with 256px and may cost GPU time. Revert kTexWidth/kTexHeight to 256 if rendering slows. Defaults for camera/viewport expansion must remain matched to avoid scaling the art.
- Z priority among three artwork layers is unchanged. No cross-card sort algorithm added.
- `tools/trace_inspection_audio.js`: optional **read-only** Frida hook for `SoundManager.GenericSoundEvent`, filtering inspection/button event names. Use only with the game binary matching RVA `0x1C9B320`, in a controlled debugging setup. This tracing script is **not automatically executed by the module**. If discard-pile reveal/hide events are sent through other sound APIs, the script will not catch them; do not treat silence as proof there are no events.
- Observe a stock discard-pile inspection open/close and a stock button press, then compare traced names. The module does not yet trigger those sounds.
- Source changes only; not compiled or Android-tested.
