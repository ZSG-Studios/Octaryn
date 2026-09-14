# Natural vegetation integration gap

Source audit only; no generated vegetation was added in the terrain-cache slice.
The active native server TerrainGeneration.cpp and client
WorldPresentation/WorldStream/GenerateColumn.cpp sample base terrain through
TerrainDensity. WorldGenerationRules.AddFeatureBlocks exists, but its only
callers found in active source are the managed rule probe. Renderable tree and
plant block assets are not evidence of generated-world integration.

Read-only original source at ref/upstream-octaryn, commit
3557cbfdc803ec034122bb55070b62b3b43b5588:

- references/old-architecture/source/world/generation/worldgen.cpp invokes the
  column flora emitter.
- features.cpp gates on lowland and grass, emits trees above normalized plant
  noise 0.8 within local coordinates 3..29, bushes above 0.55, and four flower kinds
  above 0.52. Tree trunks and surrounding two-layer leaves are explicit writes.
- noise.cpp uses Perlin fBm plant frequency 0.2, three octaves, seed offset 307.
- The current managed WorldGenerationRules mirrors this emitter's shape/rules,
  but active revision 2 terrain uses a different shared noise/density baseline.

Next restoration needs one basegame-owned feature contract reused by server
scalar queries and client bulk generation before authoritative overrides. Resolve
tree overlap/emission ordering and plant-noise choice from original code before
implementation. Verify scalar/bulk equality, signed chunk boundaries, tree/plant
IDs, support rules, explicit-air removal and reload. Preserve existing world
generation identity and saves deliberately; adding generated features silently
to an existing baseline is not a terrain-cache optimization.

The pure native TerrainFeatures.h helper and independent Features.cpp fixture
are now implemented. build/movement-ui-probes.log passes 3,167,470 checks for
thresholds, configurable IDs, signed bounds and scalar queries versus original
X-then-Z interleaved terrain/feature writes. This does not connect vegetation to
the active generator. Revision routing, plant noise, save/export identity and
client/server wiring remain outstanding; movement/F3 repairs took priority.
