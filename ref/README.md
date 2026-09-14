# References

Reference checkouts are research inputs, not linked runtime dependencies.

archives/octaryn-workspace-dev.tar.gz is the unchanged original engine archive.
Its extracted contents are now the active project root.

The complete previous C# rewrite/stress workspace and original extracted
reference remain outside the active tree:

C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace

See ../docs/development/restoration.md for provenance and verification, and
../docs/development/networking-recovery.md for the fixes worth transferring.
Do not treat preserved rewrite plans as active implementation instructions.

## Original implementation and history

`upstream-octaryn` is a separate reference checkout from
https://github.com/ZSG-Studios/Octaryn.git, pinned during inspection to
`3557cbfdc803ec034122bb55070b62b3b43b5588`. Its sparse checkout includes
`references/old-architecture` (401 tracked files). Git history is available for
read-only investigation. The development tarball omitted this directory.

This reference contains the original SDL GPU/GLSL render passes, texture atlas
handling, UI overlay, lighting controls, and runtime composition. Use it to
recover behavior into the active engine's existing owners with Slang/Vulkan.
Do not modify this checkout or substitute launching it for integration work.

## Terrain generation reference

`Pumpkin` is a separate sparse reference checkout of
[Pumpkin-MC/Pumpkin](https://github.com/Pumpkin-MC/Pumpkin), pinned to
`927b7fccd734e33c18c2c409e4caa61c4696de44` on 2026-09-13. It includes
`crates/pumpkin-world` and `crates/pumpkin-util` plus repository root files.
It is research material, not an Octaryn dependency or an engine replacement.
Keep its GPL-3.0 license with the checkout; no Pumpkin source is copied into
Octaryn's implementation. The sparse checkout omits generated `pumpkin-data`
tables and is not a complete independently buildable workspace.

See [terrain-generation.md](../docs/development/terrain-generation.md) for
inspected source links, algorithm boundaries, and the custom engine design.

To recreate the ignored local checkout from this repository root:

```powershell
git clone --filter=blob:none --sparse https://github.com/Pumpkin-MC/Pumpkin.git ref/Pumpkin
git -C ref/Pumpkin sparse-checkout set crates/pumpkin-world crates/pumpkin-util
git -C ref/Pumpkin checkout --detach 927b7fccd734e33c18c2c409e4caa61c4696de44
```
