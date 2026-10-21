# TRACE-SAFE CONFIGURATION FOR PICK3 ASSET CAPTURE

Why: The game intentionally exits when CardInspectionOverlay tries to load
autotagged/53bddfe24e1719146baf8fa4757904c4 after CustomComponents has
already opened that same UnityFS bundle through AssetBundle.LoadFromFile.
Android log 2026-10-19 18:09:14: duplicate bundle, AssetBundleHeap fails,
ApplicationKiller.ExitAppForAssetLoadFailure called.

This trace-only configuration disables ALL direct owned UnityFS
LoadFromFile/LoadAsset calls in CustomComponents, not the game's own bundles.
PickFromThree continues using verified PNG fallback art while the tracer
observes game-owned live Sprite assets.

Use together with Bloom/Pick3AssetTracer from
Bloom-Pick3AssetTracer-OnePass.zip (leave its config unchanged).
Extract over Bloom/ folder in Sprig, then rebuild/reinstall.

Expected log: CustomComponents/Pick3/UNITYFS: CONFIG enabled=0 probeMode=0
Expected log: Pick3AssetTracer: READY ...
Expected (when opening stock inspection): Pick3AssetTracer CAPTURE category=inspection,
plus original inspection FIELD/ICON_SET/COLOR_SCHEME logs when hooks trigger.
No CustomComponents/Pick3/UNITYFS: LOAD_BEGIN name=circle should occur.

The prewarmer remains active in CPU-only menu and match-time verified PNG
fallback modes. Native avatar creation remains crash-isolated.

IMPORTANT: The original failure can cause the game to delete its own cached
bundle. If the stock game asks to redownload assets, allow that to complete.
Do not delete game data or saved decks for this issue.

RESTORE: Replace CustomComponents/config.ini with a backed-up version only
after replacing the duplicate-bundle loader with a game-owned asset strategy.
Enabling ProbeMode 3 or 4 still risks a stock inspection bundle collision.
