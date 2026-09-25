"""Authored Bistro foliage cards whose filtered edges must remain cutouts.

At 1024px the Leaves/Bux atlases contain 10.2396%/10.9077% intermediate alpha,
just above the conservative 10% generic classifier threshold. Their inspected
leaf silhouettes are coverage masks, not glass; keep this override name-scoped.
"""
BISTRO_CUTOUT_MATERIALS = frozenset((
    'Foliage_Leaves.DoubleSided',
    'Foliage_Bux_Hedges46.DoubleSided',
))


def alpha_mode(material_name, classified_mode):
    return 'MASK' if material_name in BISTRO_CUTOUT_MATERIALS else classified_mode
