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

## Publication

GitHub [blocks ordinary files above 100 MiB](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github).
Imported scenes and generated caches remain local content inputs. Generated cooks, build outputs, settings and saves
also stay outside the published source tree.
