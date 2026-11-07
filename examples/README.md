# JSON example catalog

Choose a category, then use the shape column to find the insertion point. These
are authoring recipes; complete card files contain demonstration identifiers that
must be adapted to your project.

| Category | Files | Contents |
| --- | --- | --- |
| [Abilities](abilities/README.md) | 64 | Innate abilities, grants, removal, durations, and destroyable board abilities. |
| [Configuration](configuration/README.md) | 1 | INI examples for features configured outside card JSON. |
| [Deck ordering](deck-order/README.md) | 5 | Signed offsets and absolute positions for a physical deck card. |
| [Deck subset drawing](draw-effects/README.md) | 6 | Strict/fallback tutors, live-entity queries, and delivered-card heralds. |
| [PickFromThree](pick-from-three/README.md) | 32 | Selection modes, physical representatives, additional targets, and complete creation setups. |
| [Play requirements](play-requirements/README.md) | 7 | Per-ability trick gates, Level predicates, and the stock unconditional Unplayable marker. |
| [Queries](queries/README.md) | 38 | Physical identity, GUID membership, discard provenance, ordered pools, and placement. |
| [Generated state reference](reference/README.md) | 3 | Runtime marker shapes for understanding queries; do not author these as gameplay actions. |
| [Card templates and variants](templates/README.md) | 10 | Complete demonstration cards and alternate setups. Replace demonstration GUIDs and prefab metadata before use. |
| [Triggers](triggers/README.md) | 21 | Named events, built-in trigger firing, discard/return listeners, and picked-card effects. |
| [Zone effects](zone-effects/README.md) | 27 | Discard, removal, return, revive, and subset-creation options. |

## Where a fragment belongs

| Shape | Insertion point |
| --- | --- |
| Complete card | A card-data entry; replace GUID, prefab and normal metadata |
| Effect group component | Card `entity.components`; merge its effect-entity array |
| Effect entity | `EffectEntitiesDescriptor.$data.entities` |
| Trigger, filter or effect descriptor | An effect entity's `components` array |
| Query | A field such as Query, SubsetQuery, CardQuery, or a composite's queries array |
| Innate ability/BoardAbility component | The card's `entity.components` array |
| INI configuration | `config.ini`, not card JSON |

Keep grouping and ownership constraints appropriate for the card. In HeraldEntities,
`ids` are zero-based indices in the same descriptor's effect-entity array, not
AbilityGroupIds or card GUIDs. Update them if you append or reorder the entities.

## Identity and selection

Physical queries select an existing card and preserve its modifiers. GUID queries
select definitions or test whether a GUID occurs in a pool. Use
PickedFromThreeQuery for a subset payout and PickedFromThreeEntityQuery for the
actual selected/produced card. See [PF3](../docs/PICKFROMTHREE.md) and
[query reference](../docs/QUERIES.md).

For card-first PF3 followed by OpenLaneForCardQuery(PickedFromThree), lane finding
runs after selection. Set AdditionalTargetFirst to reverse interactive ordering.
Subset creation can grant Team-Up/Aquatic and choose Front/Back; false grant flags
preserve innate traits. See [zone effects](../docs/ZONE_EFFECTS.md).

The generated-state reference documents engine-written markers. It is not a
recipe for bypassing the discard, selection or transfer systems.
