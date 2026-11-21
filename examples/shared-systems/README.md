# Shared system examples

These snippets are not complete cards. See the [field guide](../../docs/LANES_DEATH_SUPERPOWERS.md)
for interactions, timing and query semantics.

| File | Where to put it |
| --- | --- |
| [attack-also-in-lanes](attack-also-in-lanes.json) | Card components; repeated lane 2 demonstrates deduplication |
| [attack-only-in-lanes](attack-only-in-lanes.json) | Card components; replaces other attack-lane markers |
| [cancel-death](cancel-death.json) | An effect entity listening to PreDiscardFromPlayTrigger and targeting the marked card |
| [create-superpower](create-superpower.json) | An effect entity targeting the recipient hero/player |
| [primary-target-guids](primary-target-guids.json) | A definition/subset query after ability group 0 made its primary selection |
| [superpower-deck-empty](superpower-deck-empty.json) | A query on a player or an owned card |
| [ignore-cheats-tag](ignore-cheats-tag.json) | A subset query's CompositeAllQuery, alongside its real card restrictions |
| [triggered-grant-options](triggered-grant-options.json) | Extra fields inside an existing, working GrantTriggeredAbilityEffectDescriptor's $data |

The grant-options file deliberately contains only extension fields. Retain your
actual AbilityGuid and value fields; no invented ability GUID is supplied.
Death cancellation is a trigger effect, not a general heal. Add your intended
condition or charge limit when authoring a save-once ability.