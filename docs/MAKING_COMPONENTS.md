# Making a component

Start with one behavior and one small card that demonstrates it. Before writing a hook, find the stock component that already does the closest thing. Reusing its effect queue, placement helper or history recorder usually gives you the right triggers and animations too.

## Follow the game from input to presentation

A descriptor describes an action; a runtime effect carries it through simulation. Queries choose valid entities. History records tell the frontend what happened. Keep these roles separate. A targeting query must not create cards or change the board.

Use `sprig.ps1 find Type.Member` in the Sprig root to check every game name and signature. Read Sprig's `docs/agents/card-dsl.md`, `bnm-classes.md` and `hooks.md` before adding a managed type or hook.

## Implement the complete lifecycle

Declare the managed type with BNM and reserve its base class's full instance layout. Implement copying and pool return for each added field. Copy owned queries through the game's lifecycle; never share a pooled query between owners. Store managed references through the write barrier and retain deferred objects with GC handles.

A new `.ctor` must explicitly call `Core::NativeConstruction::Base(this)` before initializing its own fields. BNM's origin-call macro has no constructor origin on a new class: it does not initialize the native base. Runtime effects need that base constructor for their data and context, even if their added fields are empty.

Keep the selected runtime target separate from the saved event context. For example, creating a card in a player's deck selects the player, while a subset query may still refer to the card that died. `Engine::EffectEventContext` provides that original event context without changing the selected destination.

Resolve metadata at initialization and check every required lookup. Bind a copy of a cached BNM field or method before selecting an instance: its indexing operator changes the handle's receiver. Simulation and presentation can run on different threads.

Give each stock method one hook owner. Add an observer to an existing owner when several features need that boundary. Use exact hook signatures, including the trailing method argument, and check the original pointer after installation.

## Keep the implementation readable

Keep registration, public types and the system's entry points in its header. Move a substantial internal stage into a named `detail/*.inl` file when that makes the flow easier to follow. State where the fragment is included and which namespace owns it. Leave short helpers together with their callers.

Write one minimal JSON example with the required fields, defaults and target meaning. Add a second example only when it demonstrates a distinct behavior. Link both from the relevant feature guide.

## Check the result

Run `sprig.ps1 lint CustomComponents`, then build the module. Test a normal case, an empty or invalid selection, a prediction clone, and a pooled reuse. Check that history and animations agree with the model. Record the build and device checks honestly; compilation cannot establish gameplay correctness.
