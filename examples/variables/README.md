# Variables and SelfTeleport examples

Read the [variables guide](../../docs/VARIABLES.md) and [SelfTeleport guide](../../docs/SELF_TELEPORT.md) for semantics and limits. These are insertion fragments, not complete cards.

| Example | Shape and purpose |
| --- | --- |
| [add-int](add-int.json) | SetVariableEffectDescriptor: add int |
| [aggregate-multiplier](aggregate-multiplier.json) | Effect component: sum a variable across selected friendly fighters |
| [append-entity](append-entity.json) | SetVariableEffectDescriptor: append entity |
| [array-contains](array-contains.json) | Query: scalar membership in an array |
| [capture-query-entities](capture-query-entities.json) | SetVariableEffectDescriptor: capture query entities |
| [capture-query-guids](capture-query-guids.json) | SetVariableEffectDescriptor: capture query guids |
| [clear-array](clear-array.json) | SetVariableEffectDescriptor: clear array |
| [compare-expressions](compare-expressions.json) | Query: arrays and expressions on both sides |
| [compare-guid](compare-guid.json) | Query: typed GUID equality |
| [compare-number](compare-number.json) | Query: numeric expression against a threshold |
| [conditional-self-teleport](conditional-self-teleport.json) | EffectEntitiesDescriptor: board count on Self and a conditional permission at three zombies |
| [continuous-deck-discard](continuous-deck-discard.json) | EffectEntitiesDescriptor: source activity across all four physical card zones |
| [continuous-query-count](continuous-query-count.json) | EffectEntitiesDescriptor: derived count without a multiplier |
| [copy-array](copy-array.json) | SetVariableEffectDescriptor: copy array |
| [defaults](defaults.json) | Card component: typed defaults, including empty entity arrays and negative integers |
| [draw-remembered-definition](draw-remembered-definition.json) | EffectEntitiesDescriptor: later draw from that stored GUID on destruction |
| [grant-self-teleport](grant-self-teleport.json) | Effect descriptor: temporary grant to its runtime target |
| [has-variable](has-variable.json) | Query: presence, including zero or empty values |
| [on-play-counter](on-play-counter.json) | EffectEntitiesDescriptor: increment on its own play trigger |
| [owner-query](owner-query.json) | SetVariableEffectDescriptor: owner query |
| [read-array-element](read-array-element.json) | SetVariableEffectDescriptor: read array element |
| [read-array-length](read-array-length.json) | SetVariableEffectDescriptor: read array length |
| [remember-pf3-definition](remember-pf3-definition.json) | EffectEntitiesDescriptor: store a database PF3 GUID on Self; replace the demo plant subset |
| [remove-array-index](remove-array-index.json) | SetVariableEffectDescriptor: remove array index |
| [remove-array-value](remove-array-value.json) | SetVariableEffectDescriptor: remove array value |
| [remove-self-teleport](remove-self-teleport.json) | Effect descriptor: remove the permission, including innate state |
| [remove-variable](remove-variable.json) | SetVariableEffectDescriptor: remove variable |
| [self-teleport](self-teleport.json) | Card component: innate card-local permission |
| [set-expression](set-expression.json) | SetVariableEffectDescriptor: set expression |
| [set-int](set-int.json) | SetVariableEffectDescriptor: set int |
| [snapshot-attack](snapshot-attack.json) | SetVariableEffectDescriptor: snapshot attack |
| [snapshot-cost](snapshot-cost.json) | SetVariableEffectDescriptor: snapshot cost |
| [snapshot-health](snapshot-health.json) | SetVariableEffectDescriptor: snapshot health |
| [subtract-int](subtract-int.json) | SetVariableEffectDescriptor: subtract int |
| [variable-card-query](variable-card-query.json) | Query: match the definition remembered on Self |
| [variable-multiplier](variable-multiplier.json) | Effect component: expression read on Self; place beside an amount descriptor |

Setters default to `Owner: Self` in these examples. Their `Target` and query-based captures still use the surrounding effect context. Adjust the primary target filter, trigger and owner for your card. Array copies require the named source variable to exist; clear/remove operations do not create a missing array.

The conditional example counts zombies rather than hard-coding a science subtype ID. To require science zombies, add your card database’s science subtype query to its QueryMultiplier query. Keep reactive dependencies acyclic.
