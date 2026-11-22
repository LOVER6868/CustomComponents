# Transform into an existing fighter

Use `TransformCardIntoExistingCardEffectDescriptor` with a primary fighter filter for the card that changes and a secondary fighter filter for its donor. Both selections are in play. The fighter keeps its entity ID, owner and lane through the stock transform.

| Field | Default | Behavior |
| --- | --- | --- |
| `MakeACopy` | `false` | Keep the donor when true. Otherwise remove it after the transform without firing death or discard triggers. Both modes use the donor’s base definition. |
| `PluckFailSafe` | `false` | Consult `SubsetQuery` when the effect reaches execution without a valid donor. |
| `SubsetQuery` | Unset | Card definition query for the fallback. The stock picker checks the destination lane. |
| `FirePlayTrigger` | `false` | Fire a play trigger after transformation and any donor removal. |

A transform in place emits no move trigger. The recorder uses the stock transform record and a separate donor-removal record in consume mode. Live buffs and temporary traits on the donor do not carry over.

[Copy](../../examples/new-card-features/transform-copy-fighter.json) · [Consume](../../examples/new-card-features/transform-consume-fighter.json) · [Subset fallback](../../examples/new-card-features/transform-copy-or-fallback.json).
