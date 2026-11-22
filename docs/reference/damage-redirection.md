# Damage redirection

Use the stock `DamageEffectRedirector`, `DamageEffectRedirectorDescriptor`, trigger filters and primary target filter. The stock system chooses a replacement, copies damage context, queues its effects and records the result.

During stock damage redirection, `InFrontSameLaneQuery` also supports an environment as `Self`: it selects that environment owner's front fighter in its lane. It excludes the incoming victim, because redirecting damage to that same entity is not a replacement. Fighter-source behavior remains stock. Filters such as `InSameLaneQuery`, faction checks and targetability still apply; the extension does not invent a destination when no legal fighter exists.

Test hero damage, back-fighter damage, damage to the front fighter itself, an empty lane, team-up order and multiple redirectors. `DamageRedirect/SELECT` reports the selected effect entity; damage and history remain owned by the stock system.
