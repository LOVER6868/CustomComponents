# Return to a lane

`ReturnFromDiscardPileToLaneEffectDescriptor` returns the original discarded fighter or environment. Primary selects the card; secondary selects a lane. Fighters obey capacity/Team-Up rules and require active Aquatic for water. Environments use the stock environment slot/replacement path. Returns emit play/return events and suppress return-origin MoveTrigger. Card-specific ownership and terrain constraints remain in authored queries. The compatibility ForceFaceDown field does not change the face-up fighter return path.

See the [examples or related guide](../../examples/zone-effects/README.md) and [feature catalog](../FEATURES.md).
