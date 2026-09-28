"""CPU numerical checks for reflection sample centers and strict history search."""
import json
import math


def sample_ndc(pixel, source, history):
    texel = [min(int((pixel[k]+.5)*source[k]/history[k]), source[k]-1) for k in range(2)]
    return [(texel[0]+.5)/source[0]*2-1, 1-(texel[1]+.5)/source[1]*2]


def select(coordinate, extent, valid):
    base = [math.floor(value-.5) for value in coordinate]
    candidates = [(base[0]+(i&1),base[1]+(i>>1)) for i in range(4)]
    candidates = [p for p in candidates if all(0 <= p[k] < extent[k] for k in range(2)) and valid(p)]
    return min(candidates, key=lambda p: sum((p[k]+.5-coordinate[k])**2 for k in range(2)), default=None)


def main():
    count = 0
    legacy_error = 0
    max_error = 0
    for source in [(2560,1440),(3840,2160),(2561,1441),(641,361)]:
        for divisor in (1,2,3):
            history = [math.ceil(value/divisor) for value in source]
            # Prior projection must keep the raster source aspect, even when
            # ceil-divided history dimensions have a different aspect.
            fy = 1/math.tan(math.radians(60)/2)
            fx = fy*source[1]/source[0]
            for pixel in [(0,0),(history[0]//2,history[1]//2),(history[0]-1,history[1]-1)]:
                ndc = sample_ndc(pixel, source, history)
                old = [(pixel[0]+.5)/history[0]*2-1, 1-(pixel[1]+.5)/history[1]*2]
                offset = max(abs(ndc[k]-old[k])*source[k]/2 for k in range(2))
                if divisor == 1:
                    assert offset == 0, 'Native reference sample centers changed'
                legacy_error = max(legacy_error, offset)
                for jitter in [(0,0),(.25,-.25),(-.4375,.38888889)]:
                    clip_jitter = [2*jitter[0]/source[0], -2*jitter[1]/source[1]]
                    for depth in (1,10,1000):
                        ray = [(ndc[0]-clip_jitter[0])/fx,(ndc[1]-clip_jitter[1])/fy,1]
                        world = [component*depth for component in ray]
                        projected = [world[0]/world[2]*fx+clip_jitter[0],
                                     world[1]/world[2]*fy+clip_jitter[1]]
                        error = max(abs(projected[k]-ndc[k]) for k in range(2))
                        max_error = max(error,max_error)
                        assert error < 1e-12
                        count += 1
    assert legacy_error >= .499, 'Fixture must expose the old reduced-grid mismatch'
    extent = (32,24)
    # A rejected nearest texel must never prevent a different valid footprint tap.
    assert select((4.9,5.1),extent,lambda p:p==(5,5)) == (5,5)
    assert select((4.9,5.1),extent,lambda p:False) is None
    assert select((4.9,5.1),extent,lambda p:True) == (4,5)
    assert select((0,0),extent,lambda p:True) == (0,0)
    assert select((-1,-1),extent,lambda p:True) is None
    # A prior foreground occluder cannot supply the newly exposed plane's history.
    def same_plane(pixel, depth):
        ray = ((pixel[0]+.5)/extent[0]*2-1,1-(pixel[1]+.5)/extent[1]*2,1)
        expected = [component*10 for component in ray]
        old = [component*depth for component in ray]
        return math.dist(expected,old) < .025
    assert select((4.9,5.1),extent,lambda p:same_plane(p,9)) is None
    assert select((4.9,5.1),extent,lambda p:same_plane(p,10 if p==(5,5) else 9)) == (5,5)
    print(json.dumps(dict(round_trip_cases=count,maximum_ndc_error=max_error,
                          legacy_reduced_grid_error_source_pixels=legacy_error,
                          selection_cases=7,scope='CPU equations; shader and GPU qualification still required')))


if __name__ == '__main__':
    main()
