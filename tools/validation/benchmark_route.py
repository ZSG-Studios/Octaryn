"""Resolve and verify an explicit render-only streaming benchmark origin."""
import hashlib
import json
import math
import struct


def float32(value):
    if isinstance(value, bool) or not isinstance(value, (float, int)):
        raise ValueError('Route coordinates must be finite numbers')
    try:
        if not math.isfinite(value):
            raise ValueError('Route coordinates must be finite numbers')
        result = struct.unpack('<f', struct.pack('<f', value))[0]
    except (OverflowError, struct.error) as error:
        raise ValueError('Route coordinate exceeds finite float32 range') from error
    if not math.isfinite(result):
        raise ValueError('Route coordinate exceeds finite float32 range')
    return result


def validate_origin(xyz, travel, radius):
    distance = float32(travel)
    if distance < 0 or not isinstance(radius, int) or isinstance(radius, bool) or radius < 0:
        raise ValueError('Route requires nonnegative travel and an integer radius')
    endpoint = [*xyz[:2], float32(xyz[2] - distance)]
    # Match native signed block coordinates, full streaming window, preload and alignment.
    padding = (radius + 2) * 32
    minimum, maximum = -(2 ** 31) + padding, 2 ** 31 - 1 - padding
    if any(value < minimum or value > maximum for point in (xyz, endpoint) for value in point):
        raise ValueError('Route origin and endpoint must fit signed block coordinates with streaming padding')
    return xyz


def resolve_origin(world, explicit=None, travel=0.0, radius=128):
    if explicit is not None:
        if len(explicit) != 3:
            raise ValueError('Route origin requires exactly X Y Z')
        return dict(source='explicit_absolute_eye', xyz=validate_origin([float32(value) for value in explicit], travel, radius))
    path = world / 'player_1.json'
    if not path.is_file():
        raise ValueError('Moving benchmark requires saved player_1.json or --route-origin X Y Z')
    data = path.read_bytes()
    player = json.loads(data.decode('utf-8-sig'))
    if not isinstance(player, dict) or type(player.get('version')) is not int or player['version'] != 1:
        raise ValueError('Unsupported saved player version for benchmark origin')
    xyz = [float32(player[axis]) for axis in ('x', 'y', 'z')]
    xyz[1] = float32(xyz[1] + 48.0)
    return dict(source='saved_player_eye_plus_48', player_sha256=hashlib.sha256(data).hexdigest(), xyz=validate_origin(xyz, travel, radius))


def origin_arguments(origin):
    return ['--benchmark-route-origin', *(format(value, '.9g') for value in origin['xyz'])]


def verify_origin(rows, origin, travel):
    if not rows or origin is None:
        raise ValueError('Missing explicit route origin or camera evidence')
    x, y, z = origin['xyz']
    end_z = float32(z - float32(travel))
    # The camera CSV emits six decimal places; this only permits serialization rounding.
    tolerance = .000002
    def same(value, expected):
        return math.isfinite(value) and abs(value - expected) <= tolerance
    for row in rows:
        if not same(float(row['camera_x']), x) or not same(float(row['camera_y']), y):
            raise ValueError('Camera X/Y differs from explicit route origin')
    if not same(float(rows[0]['camera_z']), z):
        raise ValueError('Initial camera Z differs from explicit route origin')
    if not same(float(rows[-1]['camera_z']), end_z):
        raise ValueError('Final camera Z differs from explicit route endpoint')
    return dict(**origin, expected_endpoint=[x, y, end_z], serialization_tolerance_metres=tolerance)
