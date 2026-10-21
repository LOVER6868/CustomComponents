# Active PickFromThree static sprites

Run `python tools/regenerate_pick3_static_embeds.py` after editing an asset.

**Verified original game sprites**: `inspection_grass` (frame_circle_grass),
`inspection_reference_rim` (smallinspectionframe), `cost_sun` (inhnd_sun_gb),
and `health_heart` (heart). These are embedded once and reused, without
scene searches or downloaded asset dependencies.

**Unverified legacy project sprites kept for functionality only**:
`description_plate`, `cost_brain`, `attack_leaf`, and `inline_tooltip_plate`.
These are NOT claimed to be stock artwork. Their removal or replacement
requires additional verified game resources; deleting them now would leave
important labels/icons without a readable background. The faux green
Choose/Selected button artwork has been removed; a neutral UI button remains
clickable until a verified stock button can be extracted.

The genuine circle is intentionally NOT recolored: class flags still log
correctly, but original class-color composition is pending further reverse
engineering. The background always shows even if portrait art is unavailable.
