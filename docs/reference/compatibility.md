# Compatibility and testing

CustomComponents targets the EngineLib and Sprig/BNM layout used by this checkout. Different game or dependency builds can move native methods or change fields, so method signatures must be checked again before using a new APK.

Both PvP players need matching native modules, card data and settings. Physical-card selection, Level payment, and custom effects can otherwise disagree between simulation and client playback.

Some boundaries are easy to overlook: placement queries test board fit but not sun affordability; database PickFromThree choices are definitions, not physical cards; Team-Up/Aquatic placement still follows stock lane restrictions. `StatIcons` needs user-provided PNG files and unused icon slots.

**Not yet confirmed on a device:** every OverhealToHealth buff trigger and visual update; both move triggers and simultaneous animation for `SwapPositionEffectDescriptor`; and cached UI updates after changing tags/subtypes. Host compilation and JSON checks are useful, but they do not prove these visual or event behaviors.

Test a normal case, a rejected/blocked case, and a replay/PvP case before relying on a new native effect. See [healing and swapping](healing-and-swap.md) for the important checks.
