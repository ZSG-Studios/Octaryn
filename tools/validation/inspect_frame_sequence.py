"""Measure captured consecutive frames; numerical stability is not visual acceptance."""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def difference(first, second):
    delta = ImageChops.difference(first, second)
    bands = delta.split()
    peak = ImageChops.lighter(ImageChops.lighter(bands[0], bands[1]), bands[2])
    histogram = peak.histogram()
    return dict(mean_absolute_channel_error=sum(ImageStat.Stat(delta).mean) / 3,
                fraction_pixels_over_20=sum(histogram[21:]) / (delta.width * delta.height))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    files = [args.directory / 'frame.bmp'] + sorted(
        args.directory.glob('frame.bmp.sample-*.bmp'),
        key=lambda path: int(path.name.split('sample-')[1].split('.')[0]))
    if len(files) < 3:
        raise RuntimeError('At least three captures are required for adjacent/same-parity comparison')
    images = [Image.open(path).convert('RGB') for path in files]
    if len({image.size for image in images}) != 1:
        raise RuntimeError('Capture dimensions changed within the sequence')
    records = []
    # Exclude the top HUD strip and the center reticle from the sampling area.
    width, height = images[0].size
    crops = [image.crop((0, height // 5, width // 2 - 16, height)) for image in images]
    for index, (path, image, crop) in enumerate(zip(files, images, crops)):
        image.save(path.with_suffix('.png'))
        record = dict(file=path.name, mean_rgb=ImageStat.Stat(crop).mean)
        if index:
            record['adjacent'] = difference(crop, crops[index - 1])
        if index > 1:
            record['same_parity'] = difference(crop, crops[index - 2])
        records.append(record)
    thumb_width = 480
    thumb_height = max(1, round(height * thumb_width / width))
    sheet = Image.new('RGB', (thumb_width * 2, thumb_height * ((len(images) + 1) // 2)))
    for index, image in enumerate(images):
        sheet.paste(image.resize((thumb_width, thumb_height)),
                    ((index % 2) * thumb_width, (index // 2) * thumb_height))
    sheet.save(args.directory / 'sequence.png')
    result = dict(acceptance='Requires visual review and capture-frame metadata; animation can change pixels',
                  region=[0, height // 5, width // 2 - 16, height], frames=records)
    (args.directory / 'image-difference.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
