# Architecture

JSON-facing components define intent. Adapters preserve it through serialization,
copying, pooling and stock execution. Feature systems own gameplay rules; shared
services own native hooks and cross-cutting lifecycle work.

## Selection lifecycle

Stock finding supplies an offer tied to a model, effect, group and generation.
The frontend presents it and hands the choice to a retained stock command.
Acceptance creates a model-owned transaction. Descendants and listeners retain
verified links to it.

Database choices begin as GUIDs. The produced-card adapter checks the new card's
GUID, entity and model before binding it and releasing physical-card listeners.
A UI receipt or speculative snapshot is not proof of a gameplay commit.
Provisional placement context is confined to read-only target evaluation.

## Shared extension points

- `HookRegistry`: one native owner per method, with observers for other consumers.
- `TriggerQueue` and `EffectQueue`: insertion boundaries, provenance and event filters.
- `SubsetSelection`: finder context, candidate rules and creation capabilities.
- `AbilityRegistry`: shared grant/remove lifecycle with ability-specific behavior.
- `FrameIconRegistry` and card-data observers: cooperative presentation extensions.
- `FrontendTickDispatcher`: hand/view work, stock input, then presentation observers.

## Lifetime and ownership

Retained managed objects use roots; simulations use model-generation tokens.
Stock-object extensions use weak sidecars with copy/pool-return handling. Do not
resize stock managed classes to add fields. Deep-copy pooled query children and
return them through stock pool utilities. Do not retain borrowed lists or choose
the first matching GUID to recover a missing physical identity.

Return-to-lane checks membership and physical legality before removal, then uses
stock placement/history. Return-origin movement is filtered from MoveTrigger.
Presentation follows its owning view/history work instead of duplicating effects.
See [source layout](SOURCE_LAYOUT.md) and [hook ownership](HOOK_OWNERSHIP.md).
