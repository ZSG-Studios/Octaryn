"""Validate the explicit authored collapsed-plane interchange contract."""
import numpy as np


def declared(material):
    value = material.get('extras', {}).get('octaryn_lighting_basis')
    if value is None:
        return False
    if value != {'version': 1, 'mode': 'zero-tangent-plane'} or type(value.get('version')) is not int:
        raise ValueError('Invalid authored lighting basis declaration')
    if material.get('alphaMode', 'OPAQUE') != 'OPAQUE' or 'normalTexture' not in material:
        raise ValueError('Zero tangent plane requires opaque material and original normal texture')
    if 'KHR_materials_unlit' in material.get('extensions', {}) or 'octaryn_material_layers' in material.get('extras', {}):
        raise ValueError('Zero tangent plane requires an unlayered lit material')
    return True


def validate(source, primitive, attributes, indices):
    material = source.doc.get('materials', [])[primitive['material']] if 'material' in primitive else {}
    zero = declared(material)
    names = ('_OCTARYN_SOURCE_TANGENT', '_OCTARYN_SOURCE_BITANGENT')
    if not zero:
        if any(name in attributes for name in names):
            raise ValueError('Authored source basis requires explicit material declaration')
        return False
    if 'TANGENT' in attributes or any(name not in attributes for name in (*names, 'NORMAL', 'TEXCOORD_0')):
        raise ValueError('Missing authored zero-plane attributes or substituted TANGENT')
    for name in names:
        accessor = source.doc['accessors'][primitive['attributes'][name]]
        if accessor['componentType'] != 5126 or accessor['type'] != 'VEC3' or accessor.get('normalized', False):
            raise ValueError('Source tangent plane requires float32 vec3')
        if np.any(attributes[name] != 0):
            raise ValueError('Source tangent plane must retain exact zero T/B')
    n = attributes['NORMAL'].astype(np.float64)
    uv = attributes['TEXCOORD_0'].astype(np.float64)
    if not np.all(np.isfinite(n)) or np.any(np.sum(n*n, axis=1) < 1e-16) or not np.all(np.isfinite(uv)):
        raise ValueError('Invalid authored source normal or UV')
    corners = uv[indices.reshape((-1, 3))]
    a, b = corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]
    if np.any(a[:, 0]*b[:, 1]-a[:, 1]*b[:, 0] != 0):
        raise ValueError('Zero-plane source requires degenerate UV triangles')
    return True
