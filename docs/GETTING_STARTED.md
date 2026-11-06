# Getting started

## Install and build

1. Place this directory at `Sprig-main/Bloom/CustomComponents`.
2. From PowerShell, apply the included BNM dependency fix:

   ```powershell
   .\patches\BNM-Android\Apply.ps1 -SprigRoot 'C:\path\to\Sprig-main'
   ```

   The installer verifies the dependency version and its output. It accepts
   `-WhatIf`. See [the dependency guide](../patches/BNM-Android/README.md) if the
   checked dependency has local modifications.
3. Build Sprig's native library and package the APK through your Sprig setup.
   The module targets the Android ARM64 game build used by Sprig/Bloom.
4. Install matching native code and rebuilt card data on participating PvP clients.

Bloom identifies this as a PnP module through `bloom.ini`. A clean checkout builds
without developer tests, tools or source regenerators. Keep the included generated
C++ assets under `src/`.

## Add a component

Start with a small [example](../examples/README.md). Component fragments belong in
an `entity.components` array. Triggers, filters and effects usually belong on
effect entities inside the card's `EffectEntitiesDescriptor`. Queries belong in
fields such as `Query`, `SubsetQuery` and `CardQuery`.

A complete card file is a template. Assign its GUID, prefab and normal metadata
for your project instead of nesting the entire file inside another component.
The example indexes identify each file's shape.

Check that startup reaches the module's ready message. For PF3,
`CREATION_BINDING: ready=1` confirms that its draw/creation adapters resolved.
Missing dependencies and unresolved hooks are reported during initialization.
