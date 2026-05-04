DEFAULT_PACK_URL = (
    "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/archive/refs/heads/"
    "java-latest.zip"
)
DEFAULT_PACK_FILE_NAME = "Classic-32x-Jappa-Java-java-latest.zip"
DEFAULT_PACK_NAME = "Classic Faithful 32x Jappa"
DEFAULT_PACK_SOURCE_URL = "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java"
DEFAULT_PACK_LICENSE_NAME = "Faithful License Version 3"
DEFAULT_PACK_LICENSE_URL = "https://faithfulpack.net/license"
DEFAULT_PACK_CREDIT = "Faithful Resource Pack and ClassicFaithful contributors"
DEFAULT_PACK_CREDITS_URL = (
    "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/blob/main/credits.txt"
)
DEFAULT_PACK_LICENSE_FILE = "license.txt inside the source pack"
FALLBACK_PACK_NAME = "Octaryn fallback atlas"
FALLBACK_PACK_SOURCE_URL = "octaryn-basegame/Tools/build_atlas_from_pack.py --fallback-only"
FALLBACK_PACK_LICENSE_NAME = "Octaryn project source license"
FALLBACK_PACK_LICENSE_URL = "docs/THIRD_PARTY_NOTICES.md"
FALLBACK_PACK_CREDIT = "Octaryn generated fallback colors"
TEXTURE_PACK_LICENSE_NOTICE = (
    "Generated atlases may contain third-party texture pack work. Verify distribution "
    "rights, preserve required credit, and include the upstream license file before release."
)

ATLAS_SOURCES = {
    1: ("grass_block_top", ["grass_block_top.png"]),
    2: ("grass_block_side", ["grass_block_side.png"]),
    3: ("dirt", ["dirt.png"]),
    4: ("stone", ["stone.png"]),
    5: ("sand", ["sand.png"]),
    6: ("snow", ["snow.png", "snow_block.png"]),
    7: ("oak_log_top", ["oak_log_top.png"]),
    8: ("oak_log", ["oak_log.png", "oak_log_side.png"]),
    10: ("oak_leaves", ["oak_leaves.png"]),
    11: ("rose", ["poppy.png", "rose_bush_top.png", "red_tulip.png"]),
    12: ("gardenia", ["oxeye_daisy.png", "white_tulip.png"]),
    13: ("bluebell", ["cornflower.png", "blue_orchid.png"]),
    14: ("lavender", ["allium.png", "lilac_top.png"]),
    15: ("bush", ["short_grass.png", "grass.png", "fern.png"]),
    16: ("water", ["water_still.png"]),
    17: ("red_torch", ["redstone_torch.png", "torch.png"]),
    18: ("green_torch", ["torch.png"]),
    19: ("blue_torch", ["torch.png", "soul_torch.png"]),
    20: ("yellow_torch", ["torch.png"]),
    21: ("cyan_torch", ["soul_torch.png", "torch.png"]),
    22: ("magenta_torch", ["torch.png"]),
    23: ("white_torch", ["torch.png"]),
    24: ("oak_planks", ["oak_planks.png"]),
    25: ("glass", ["glass.png"]),
    26: ("water_flow", ["water_flow.png"]),
    27: ("lava_still", ["lava_still.png"]),
    28: ("lava_flow", ["lava_flow.png"]),
}

SOLID_FALLBACKS = {
    1: (106, 151, 65, 255),
    2: (111, 141, 68, 255),
    3: (134, 96, 67, 255),
    4: (125, 125, 125, 255),
    5: (218, 210, 158, 255),
    6: (240, 247, 247, 255),
    7: (151, 112, 63, 255),
    8: (103, 81, 49, 255),
    9: (255, 255, 255, 160),
    10: (73, 123, 38, 210),
    16: (64, 96, 255, 155),
    24: (162, 130, 78, 255),
    25: (190, 230, 240, 96),
    26: (64, 96, 255, 155),
    27: (255, 96, 0, 255),
    28: (255, 96, 0, 255),
}

SPRITE_FALLBACKS = {
    11: (196, 42, 42, 255),
    12: (245, 245, 225, 255),
    13: (72, 116, 214, 255),
    14: (154, 92, 202, 255),
    15: (92, 151, 54, 255),
    17: (236, 39, 63, 255),
    18: (90, 181, 82, 255),
    19: (51, 136, 222, 255),
    20: (243, 168, 51, 255),
    21: (54, 197, 244, 255),
    22: (250, 110, 121, 255),
    23: (255, 255, 255, 255),
}

TORCH_TINTS = {
    17: (1.45, 0.55, 0.55),
    18: (0.65, 1.25, 0.65),
    19: (0.55, 0.80, 1.45),
    20: (1.35, 1.05, 0.55),
    21: (0.55, 1.20, 1.35),
    22: (1.35, 0.65, 1.15),
    23: (1.25, 1.25, 1.25),
}

GRASS_BIOME_TINT = (0.57, 0.74, 0.35)
OAK_LEAVES_TINT = (0.72, 0.86, 0.58)
BUSH_TINT = (0.70, 0.84, 0.52)
