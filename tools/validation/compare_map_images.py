"""Compare matching observed frames without treating pixel error as quality approval."""
import argparse
import json
import math
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
from summarize_performance_matrix import camera_records, capture_lighting, require_same_camera


def compare(reference, candidate, output):
    cameras = [camera_records(path / 'camera-motion.csv') for path in (reference, candidate)]
    require_same_camera(cameras)
    lighting = [capture_lighting(path, camera) for path, camera in zip((reference, candidate), cameras)]
    if lighting[0] != lighting[1] or not lighting[0]:
        raise ValueError('Actual captured sunlight or ready-frame identities differ')
    output.mkdir(parents=True, exist_ok=True)
    def images(case, camera):
        ready = {row[0]: key for key, row in camera.items()}
        return {ready[json.loads(path.read_text())['render_frame']]:
                Path(str(path).removesuffix('.lighting.json')) for path in case.glob('*.bmp.lighting.json')}
    paths = [images(case, camera) for case, camera in zip((reference, candidate), cameras)]
    if paths[0].keys() != paths[1].keys():
        raise ValueError('Captured ready frames differ')
    results = []
    for ready in sorted(paths[0]):
        a, b = [Image.open(path[ready]).convert('RGB') for path in paths]
        if a.size != b.size: raise ValueError('Image dimensions differ')
        difference = ImageChops.difference(a, b)
        stats = ImageStat.Stat(difference)
        histogram = difference.histogram()
        pixels = a.width * a.height
        over16 = sum(sum(histogram[channel*256+17:(channel+1)*256]) for channel in range(3))
        results.append(dict(ready_frame=ready, dimensions=a.size,
            mean_absolute_channel_error=sum(stats.mean)/3,
            rms_channel_error=math.sqrt(sum(value*value for value in stats.rms)/3),
            channel_fraction_over_16=over16/(pixels*3)))
        # Nearest-neighbor thumbnails preserve the shape of artifacts for inspection.
        views = [a, b, difference.point(lambda value: min(255, value*8))]
        width = min(960, a.width); height = round(a.height*width/a.width)
        sheet = Image.new('RGB', (width*3, height))
        for index, view in enumerate(views):
            sheet.paste(view.resize((width,height), Image.Resampling.NEAREST), (index*width,0))
        sheet.save(output / f'frame-{ready}.png')
    report = dict(reference=str(reference), candidate=str(candidate), frames=results,
                  panel_order=['reference','candidate','absolute difference x8'],
                  acceptance='Requires visual and motion review; metrics are not perceptual acceptance')
    (output / 'comparison.json').write_text(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(compare(args.reference,args.candidate,args.output), indent=2))
