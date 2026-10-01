# Octaryn map sources

Large imported scenes are local content inputs, not normal Git payloads.
Attribution, source notes and manifests are tracked; imported geometry and runtime
caches remain local. Personal world sources can also stay outside this directory
and be registered through Add World in the library.

## Bistro

The development `map.json` references a locally prepared `main.glb`. Restore that
file from the existing asset copy before building the bundled Bistro fixture,
or prepare a source through the tools in `octaryn-client/Tools/MapImport`.
The existing prepared GLB is 464,601,528 bytes and is not published in normal Git.

Source: [Amazon Lumberyard Bistro, Open Research Content Archive (ORCA)](https://developer.nvidia.com/orca/amazon-lumberyard-bistro).
The source asset is distributed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Octaryn's FBX import, material/alpha processing and spatial preparation are
derivatives of that source. Preserve its attribution and source license when
redistributing prepared content. The official download is an FBX/Falcor scene;
it is not a byte-identical download of Octaryn's prepared GLB.

## Zorah

Acquire the geometry-only `zorah_main_public.v2.gltf.7z` export through the
[official vk_lod_clusters scene documentation](https://github.com/nvpro-samples/vk_lod_clusters/blob/main/docs/scenes.md).
Extract its GLTF and companion BIN together into `zorah` or another local folder.
Keep the provided license and README with the source. This repository retains
the source notice, MIT license and sample configuration files under `zorah`.

The current companion BIN is 10,001,629,940 bytes. It exceeds both normal GitHub
Git limits and GitHub LFS per-file limits, so it remains local. Do not replace
it with a pointer that the importer interprets as binary geometry. The GLTF
companion is also kept with the local source package rather than published alone.

Zorah preparation and full-world rendering are still unqualified. See
[scene geometry scaling](../../../docs/development/scene-geometry-scaling.md)
and the [active NVRHI rewrite](../../../docs/development/nvrhi-renderer-rewrite.md).

## Publication

GitHub [blocks ordinary files above 100 MiB](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github).
GitHub LFS [has plan-dependent per-file limits](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage),
with a maximum below this Zorah buffer. Excluding these payloads does not delete
existing local source files. Generated cooks, build outputs, settings and saves
also stay outside the published source tree.
