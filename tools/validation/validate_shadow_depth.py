#!/usr/bin/env python3
"""Numerical float32 stress test for compact shadow depth and its history guard."""
import json
import math
import struct


def f32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


def vector(values):
    return tuple(map(f32, values))


def add(a, b):
    return vector(x + y for x, y in zip(a, b))


def sub(a, b):
    return vector(x - y for x, y in zip(a, b))


def scale(a, value):
    return vector(x * value for x in a)


def dot(a, b):
    return f32(sum(f32(x * y) for x, y in zip(a, b)))


def distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def normal(a):
    return scale(a, 1 / math.sqrt(dot(a, a)))


def history_matches(old, world, eye, ray, plane_normal, tolerance):
    denominator = dot(ray, plane_normal)
    if abs(denominator) < 1e-6:
        return False
    depth = f32(dot(sub(world, eye), plane_normal) / denominator)
    return depth > 0 and distance(old, add(eye, scale(ray, depth))) < tolerance


def main():
    cases = 0
    maximum_error = 0
    large_origin_rejections = 0
    for origin in (0, 10000, 1000000):
        eye = vector((origin + .125, 3.5, -origin - .25))
        for depth in (1, 10, 100, 4096):
            for pixel in ((0, 0), (1279, 719), (2559, 1439)):
                for jitter in ((0, 0), (.00031, -.00042)):
                    clip = ((pixel[0] + .5) / 2560 * 2 - 1 - jitter[0],
                            -((pixel[1] + .5) / 1440 * 2 - 1) - jitter[1])
                    # A rotated float32 basis exercises the dot-product round trip.
                    forward = vector((math.sin(.73), 0, -math.cos(.73)))
                    right = vector((math.cos(.73), 0, math.sin(.73)))
                    ray = add(add(forward, scale(right, clip[0] / .974279)),
                              (0, f32(clip[1] / 1.732051), 0))
                    relative = scale(ray, depth)
                    original = add(eye, relative)
                    stored_depth = dot(relative, forward)
                    reconstructed = add(eye, scale(ray, stored_depth))
                    error = distance(original, reconstructed)
                    maximum_error = max(maximum_error, error)
                    tolerance = max(.025, math.sqrt(dot(relative, relative)) * .0005)
                    assert error < tolerance, (origin, depth, pixel, error)
                    # Reprojection is a ray/receiver-plane test, not mere depth proximity.
                    for grazing in (1, .1, .01, .001):
                        tangent = normal((-ray[2], 0, ray[0]))
                        plane = normal(add(tangent, scale(normal(ray), grazing)))
                        accepted = history_matches(reconstructed, original, eye, ray, plane, tolerance)
                        if origin == 1000000 and not accepted:
                            large_origin_rejections += 1
                        displaced = add(reconstructed, scale(normal(ray), max(1, tolerance * 16)))
                        assert not history_matches(displaced, original, eye, ray, forward, tolerance)
                        cases += 1
    print(json.dumps({"cases": cases, "maximum_depth_reconstruction_error": maximum_error,
                      "large_origin_grazing_history_rejections": large_origin_rejections,
                      "note": "Conservative history rejection is allowed; this is not GPU image qualification."}, indent=2))


if __name__ == "__main__":
    main()
