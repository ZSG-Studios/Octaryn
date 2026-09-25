"""Pinned reference MikkTSpace callbacks; retain per-corner tangent discontinuities."""
import ctypes

import numpy as np


class Mikk:
    def __init__(self, path):
        self.degenerate_corners = 0
        self.singular_smoothed_corners = 0
        self.zero_angle_corners = 0
        self.collapsed_derivative_corners = 0
        self.orthogonalized_corners = 0
        self.library = ctypes.CDLL(str(path))
        pointer = ctypes.c_void_p
        self.library.map_mikk.argtypes = [pointer, pointer, pointer, pointer, ctypes.c_uint, pointer]
        self.library.map_mikk.restype = ctypes.c_int

    def generate(self, positions, normals, uvs, indices):
        arrays = [np.ascontiguousarray(value, dtype=np.float32) for value in (positions, normals, uvs)]
        normals = arrays[1]
        lengths = np.linalg.norm(normals, axis=1)
        if not np.all(np.isfinite(lengths)) or np.any(lengths < 1e-8):
            raise ValueError('Invalid normals for MikkTSpace')
        # Mikk expects unit normals; do not mutate the original attribute payload.
        arrays[1] = np.ascontiguousarray(normals / lengths[:, None])
        if not all(np.all(np.isfinite(value)) for value in arrays):
            raise ValueError('Nonfinite tangent inputs')
        indices = np.ascontiguousarray(indices, dtype=np.uint32).reshape(-1)
        if len(indices) % 3 or indices.max() >= len(positions):
            raise ValueError('Invalid triangle indices')
        result = np.zeros((len(indices), 4), dtype=np.float32)
        success = self.library.map_mikk(*(a.ctypes.data for a in arrays), indices.ctypes.data,
                                        len(indices), result.ctypes.data)
        if not success or not np.all(np.isfinite(result)):
            raise ValueError('MikkTSpace generation failed')
        corner_normals = arrays[1][indices].astype(np.float64)
        corner_normals /= np.linalg.norm(corner_normals, axis=1)[:, None]
        bad_frame = np.abs(np.sum(result[:, :3] * corner_normals, axis=1)) > 1e-5
        projection_collapsed = np.zeros(len(indices), dtype=bool)
        if np.any(bad_frame):
            # Match the production loader's Gram-Schmidt frame construction,
            # retaining Mikk direction and handedness rather than source-normal edits.
            n = corner_normals[bad_frame]
            t = result[bad_frame, :3].astype(np.float64)
            t -= n * np.sum(t * n, axis=1)[:, None]
            lengths = np.linalg.norm(t, axis=1)
            collapsed = lengths < 1e-8
            projection_collapsed[np.flatnonzero(bad_frame)[collapsed]] = True
            t /= np.maximum(lengths, 1e-8)[:, None]
            t[collapsed] = 0
            result[bad_frame, :3] = t
            self.orthogonalized_corners += int(np.count_nonzero(bad_frame))
        lengths = np.linalg.norm(result[:, :3], axis=1)
        missing = lengths < 1e-8
        if np.any(missing):
            # Opposing welded contributions can cancel exactly. Re-evaluate only
            # those corners in their original face using the same reference Mikk.
            for face in np.unique(np.flatnonzero(missing) // 3):
                local_indices = indices[face * 3:face * 3 + 3].copy()
                local = np.zeros((3, 4), dtype=np.float32)
                if not self.library.map_mikk(*(a.ctypes.data for a in arrays), local_indices.ctypes.data,
                                             3, local.ctypes.data):
                    raise ValueError('Mikk isolated-face evaluation failed')
                n = corner_normals[face * 3:face * 3 + 3]
                t = local[:, :3].astype(np.float64)
                t -= n * np.sum(t * n, axis=1)[:, None]
                length = np.linalg.norm(t, axis=1)
                local[:, :3] = t / np.maximum(length, 1e-8)[:, None]
                mask = missing[face * 3:face * 3 + 3]
                valid = mask & (np.linalg.norm(local[:, :3], axis=1) > 1e-8)
                result[face * 3:face * 3 + 3][valid] = local[valid]
                self.singular_smoothed_corners += int(np.count_nonzero(valid))
            lengths = np.linalg.norm(result[:, :3], axis=1)
            missing = lengths < 1e-8
        if np.any(missing):
            triangles = indices.reshape((-1, 3))
            p = arrays[0][triangles]
            uv = arrays[2][triangles]
            area = np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1)
            a, b = uv[:, 1] - uv[:, 0], uv[:, 2] - uv[:, 0]
            determinant = np.abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
            undefined = np.repeat((area < 1e-10) | (determinant < 1e-10), 3)
            singular = np.flatnonzero(missing & ~undefined)
            if len(singular):
                face, corner = singular // 3, singular % 3
                n = arrays[1][indices[singular]]
                row = np.arange(len(face))
                points = p[face]
                edge1 = points[row, (corner + 1) % 3] - points[row, corner]
                edge2 = points[row, (corner + 2) % 3] - points[row, corner]
                edge1 -= n * np.sum(edge1 * n, axis=1)[:, None]
                edge2 -= n * np.sum(edge2 * n, axis=1)[:, None]
                product = np.linalg.norm(edge1, axis=1) * np.linalg.norm(edge2, axis=1)
                cosine = np.sum(edge1 * edge2, axis=1) / np.maximum(product, 1e-30)
                if np.any((cosine < 1 - 1e-5) & (product > 1e-20) & ~projection_collapsed[singular]):
                    raise ValueError(f'Unexplained zero reference-Mikk tangent cosine={cosine} corners={singular}')
                # Mikk weights projected corner angles; nearly coplanar authored
                # normals can round that angle to zero. Use its unweighted frame.
                det = a[face, 0] * b[face, 1] - a[face, 1] * b[face, 0]
                e1, e2 = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
                t = (e1 * b[face, 1, None] - e2 * a[face, 1, None]) / det[:, None]
                bitangent = (e2 * a[face, 0, None] - e1 * b[face, 0, None]) / det[:, None]
                t -= n * np.sum(t * n, axis=1)[:, None]
                length = np.linalg.norm(t, axis=1)
                collapsed = length < 1e-10
                if np.any(collapsed):
                    projected_b = bitangent[collapsed] - n[collapsed] * np.sum(
                        bitangent[collapsed] * n[collapsed], axis=1)[:, None]
                    t[collapsed] = np.cross(projected_b, n[collapsed]) * result[singular[collapsed], 3, None]
                    length = np.linalg.norm(t, axis=1)
                    if np.any(length < 1e-10):
                        raise ValueError('Nondegenerate singular corner has no projected UV derivatives')
                    self.collapsed_derivative_corners += int(np.count_nonzero(collapsed))
                t /= length[:, None]
                result[singular, :3] = t
                result[singular, 3] = np.where(np.sum(np.cross(n, t) * bitangent, axis=1) < 0, -1, 1)
                self.zero_angle_corners += len(singular)
                missing[singular] = False
            # Isolated degenerate UV/geometry has no defined tangent derivative.
            # Preserve its triangles and assign a deterministic perpendicular axis.
            n = arrays[1][indices[missing]]
            axis = np.eye(3, dtype=np.float32)[np.argmin(np.abs(n), axis=1)]
            t = np.cross(axis, n)
            t /= np.linalg.norm(t, axis=1)[:, None]
            result[missing, :3] = t
            self.degenerate_corners += int(np.count_nonzero(missing))
            lengths = np.linalg.norm(result[:, :3], axis=1)
        if np.any(np.abs(lengths - 1) > .002) or np.any(np.abs(result[:, 3]) != 1):
            raise ValueError(f'Invalid Mikk tangent frame: length={lengths.min()}..{lengths.max()} '
                             f'nonunit={np.count_nonzero(np.abs(lengths-1)>.002)} signs={np.unique(result[:,3])}')
        return result


def split_vertices(indices, tangents):
    records = np.empty(len(indices), dtype=[('vertex', '<u4'), ('tangent', '<u4', (4,))])
    records['vertex'] = indices
    records['tangent'] = tangents.view(np.uint32)
    unique, inverse = np.unique(records, return_inverse=True)
    return unique['vertex'], unique['tangent'].copy().view(np.float32), inverse.astype(np.uint32)
