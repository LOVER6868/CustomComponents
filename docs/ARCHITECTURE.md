# Architecture

Trick play requirements use the shared play-validation and trigger-eligibility
rules. They narrow stock decisions and record eligible abilities before payment.
Ordered effect preparation preserves this boundary before Level modifiers are
applied. The resulting state follows native model copy and pool lifecycles.

Presentation signals resolve through `FrontendServices`, which observes the live
input holder's binder. UI refreshes use that scene's signals; prediction workers
do not read private hand selections or resolve presentation services.

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

RouterContext owns weak router/model associations shared by selection and other
engine extensions. EffectGuards lets existing router owners reject an invalid
queued custom effect without installing a second native hook. Withering uses it
to check the original zone when its queued action executes.

Marker grants retain native combat markers and keep source/duration counters in
copyable, poolable grant-state components. Zone transfers share their placement
transaction while keeping deck/discard membership and recovery explicit.

PlayTransport owns command submission/serialization and offers observers to
diagnostics. Level preserves a versioned choice across command wire data, cloning,
pooling, comparison and queued play effects. EffectGuards preparations run before
native payment/play; lifecycle observers bound the scoped cost override. ZoneChanges
reports narrow source-zone transitions so cleanup does not scan every model after
every effect. Level selection and its animation remain frontend adapters.
