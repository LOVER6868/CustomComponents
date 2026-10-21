# PickFromThree: Unmasked artwork, camera overscan, button contrast

- Native CardView is still rendered to independent square 256x256 RenderTextures. The RawImage remains 1:1 and does not use a circular Unity UI mask.
- Initial per-card camera fit now reserves at least 145% orthographic half-extent. This reduces edge clipping of moving character parts but is NOT an absolute guarantee that particles emitted outside initial bounds cannot be clipped by the finite texture or viewport.
- The camera still fits a stable representative cluster of native Renderer bounds; card-specific animation hierarchies may need later inspection to reproduce stock inspection exactly.
- Rebuilt the owned embedded Choose button PNG with an opaque bright green center and white UI tint; it no longer multiplies a translucent green image against the dimmed battlefield. This does not remove any stock targeting overlay that may appear behind the mod Canvas.
- Card order is unchanged. If explicit cross-card draw ordering becomes necessary, use higher list index above lower index. Do not reorder card subtrees now because this can block touch controls and text.
- No PvP submission was enabled. Keep testing in PvE before PvP authority validation.
- Built-in static-art embeds were regenerated from choose_button.png.
- Not compiled or tested on Android in this environment.
