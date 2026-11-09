# Card presentation

The module's presentation code runs after the game has the relevant card state. It shouldn't change gameplay data just to fix a visual issue.

Discard inspection can show the exact discarded card's current stats. Custom hand/deck discards supply their herald origin, and HeraldModifiers may add a modifier summary when installed. These are separate: HeraldModifiers isn't required for discard identity.

Card frames reconcile their displayed attack, health and cost after the related history/animation updates. Level uses the stock color machinery for its cost and stat modifications; selecting a hand mode doesn't fire buff triggers. The client also refreshes legal target markers when Level changes affordability.

`StatIcons` supports custom icons for registered abilities using PNGs. The images aren't shipped automatically. Choose unused ability/icon slots and configure the file paths and scale in `config.ini`. See [stat-icons.ini](../../examples/configuration/stat-icons.ini).

The frontend should use the live holder/UI services; simulation and prediction code shouldn't reach into a live scene's input objects.
