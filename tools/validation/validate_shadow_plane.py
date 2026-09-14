"""Check PCF receiver-plane correction against independent world ray/plane hits."""
import math


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normalized(a):
    magnitude = math.sqrt(dot(a, a))
    return tuple(x / magnitude for x in a)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def scale(a, amount):
    return tuple(x * amount for x in a)


def main():
    point = (17.35, 3.47, -23.71)
    normals = [tuple(sign if axis == selected else 0 for axis in range(3))
               for selected in range(3) for sign in (-1, 1)]
    cases = false_shadows = 0
    for elevation in (.01, .1, .6, 1.3):
        forward = normalized((math.cos(elevation) * .6, -math.sin(elevation), math.cos(elevation) * .8))
        right = normalized(cross((0, 1, 0) if abs(forward[1]) < .95 else (1, 0, 0), forward))
        up = cross(forward, right)
        for normal in normals:
            nf = dot(normal, forward)
            if nf >= -1e-4:
                continue
            depth = dot(point, forward) / 2048 + .5
            for span in (64, 256, 1024):
                clip = (dot(point, right) / span, dot(point, up) / span)
                for resolution in (512, 1024):
                    pixel = (math.floor((clip[0] * .5 + .5) * resolution),
                             math.floor((-.5 * clip[1] + .5) * resolution))
                    for y in (-1, 0, 1):
                        for x in (-1, 0, 1):
                            tap = (pixel[0] + x, pixel[1] + y)
                            tap_clip = ((tap[0] + .5) / resolution * 2 - 1,
                                        1 - (tap[1] + .5) / resolution * 2)
                            correction = ((dot(normal, right) * (tap_clip[0] - clip[0]) +
                                           dot(normal, up) * (tap_clip[1] - clip[1])) * span / (nf * 2048))
                            receiver = depth - correction
                            # Independently intersect this light-space texel's world ray
                            # with the original surface plane.
                            origin = add(scale(right, tap_clip[0] * span), scale(up, tap_clip[1] * span))
                            distance = dot(normal, tuple(a - b for a, b in zip(point, origin))) / nf
                            expected = distance / 2048 + .5
                            assert abs(receiver - expected) < 1e-11, (receiver, expected)
                            assert receiver - .000004 <= expected
                            # A parallel occluder 0.4 units closer to the light still shadows.
                            assert receiver - .000004 > expected - .4 / 2048
                            false_shadows += depth - .00004 > expected
                            cases += 1
    assert cases > 500 and false_shadows > 100
    print(f"shadow_receiver_plane=passed comparisons={cases} uncorrected_false_shadows={false_shadows}")


if __name__ == "__main__":
    main()
