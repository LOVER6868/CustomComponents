# Compatibility and limits

- Native adapters target the EngineLib layout used by this Sprig/Bloom build.
  Different game/dependency revisions need metadata and ABI verification.
- Modded PvP requires matching native code, card data and deck settings.
- Interactive additional PF3 selection requires one primary target. Database
  PF3 uses the one-target query-recipient shape in its guide.
- Placement queries test board fit, not affordability or hand-play timing.
- Return-to-lane supports fighters and environments. Card-specific restrictions
  remain in authored filters. The compatibility `ForceFaceDown` field does not
  introduce a separate face-down revive implementation.
- StatIcons needs supplied PNGs and non-conflicting special-ability slots.
- Performance work is ongoing. Targeted caching and finer timing do not establish
  a universal frame-time fix for observed turn/phase stalls.
- Frontend continuation and creation grants have policy/compilation checks;
  chained PvE menus, scene transitions and trait grants still need device testing.
- Withering, attack-marker grants and physical deck deployment pass local checks;
  on-device PvE and matching-client PvP verification is still required.
- Level cycling and payment have been exercised in PvE. Two-phone play and
  prediction still require device validation. Hand styling and consecutive-menu
  refresh changes also need confirmation on a phone.
- Query-based trick play requirements pass local checks; their on-device
  multi-ability behavior and matching-client PvP still need validation.
- Tradeable is not implemented. Do not combine integrated deck creation with the
  old standalone shuffle module.

For a bug report, include the build marker, game/dependency revision, relevant JSON,
mode/client viewpoint, reproduction steps and capture logs. Keep private device
and account details out of public examples.
