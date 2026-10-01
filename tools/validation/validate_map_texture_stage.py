"""Exercise strict map cook packaging and corruption/source identity rejection."""
from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('stage', ROOT / 'octaryn-client/Tools/MapImport/StageMapTextures.py')
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)


def main():
    parent = ROOT / 'build/release-windows/tools'
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='map-stage-', dir=parent) as temporary:
        root = Path(temporary)
        source = root / 'sources'
        source.mkdir()
        map_path = source / 'fixture.glb'
        map_path.write_bytes(b'source identity fixture')
        cache_root = root / 'cache'
        cache = cache_root / 'fixture.glb.textures'
        cache.mkdir(parents=True)
        output = root / 'bundle'
        def rejected():
            try:
                stage.stage(cache_root, source, output)
            except ValueError:
                return
            raise AssertionError('invalid cook accepted')
        rejected()
        key = 'a' * 64
        payload = cache / (key + '.dds')
        payload.write_bytes(b'DDS ' + bytes(144))
        receipt = cache / (key + '.dds.sha256')
        receipt.write_text(hashlib.sha256(payload.read_bytes()).hexdigest())
        manifest = cache / 'map-texture-cook.json'
        document = dict(version=stage.CACHE_VERSION, status='complete',
                        map_sha256=stage.digest(map_path), files=[key])
        manifest.write_text(json.dumps(document))
        assert stage.stage(cache_root, source, output) == 1
        payload.write_bytes(b'corrupt' + bytes(141))
        rejected()
        receipt.write_text(hashlib.sha256(payload.read_bytes()).hexdigest())
        map_path.write_bytes(b'changed source identity')
        rejected()
        document['map_sha256'] = stage.digest(map_path)
        document['status'] = 'pilot'
        manifest.write_text(json.dumps(document))
        rejected()
        document.update(status='complete', files=['../escape'])
        manifest.write_text(json.dumps(document))
        rejected()
    print('map_texture_stage_tests passed=1')


if __name__ == '__main__':
    main()
