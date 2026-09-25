"""Memory-mapped GLB accessors and incremental candidate writing (no image decode)."""
import copy
import hashlib
import json
import mmap
from pathlib import Path
import struct

import numpy as np

DTYPES = {5120: 'i1', 5121: 'u1', 5122: '<i2', 5123: '<u2', 5125: '<u4', 5126: '<f4'}
WIDTHS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


class Glb:
    def __init__(self, path):
        self.file = Path(path).open('rb')
        self.data = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
        magic, version, length, size, kind = struct.unpack_from('<5I', self.data)
        if magic != 0x46546c67 or version != 2 or length != len(self.data) or kind != 0x4e4f534a:
            raise ValueError('Invalid GLB header')
        self.doc = json.loads(self.data[20:20 + size])
        binary_size, binary_kind = struct.unpack_from('<II', self.data, 20 + size)
        self.offset = 28 + size
        if binary_kind != 0x004e4942 or self.offset + binary_size != length:
            raise ValueError('Expected one embedded BIN chunk')

    def accessor(self, index):
        a = self.doc['accessors'][index]
        if 'sparse' in a or 'bufferView' not in a or a['type'] not in WIDTHS:
            raise ValueError('Cook requires dense scalar/vector accessors')
        view = self.doc['bufferViews'][a['bufferView']]
        if view.get('buffer', 0) != 0:
            raise ValueError('External buffers unsupported by bounded cook')
        dtype = np.dtype(DTYPES[a['componentType']])
        width = WIDTHS[a['type']]
        offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = view.get('byteStride', dtype.itemsize * width)
        end = offset + max(0, a['count'] - 1) * stride + dtype.itemsize * width
        if a['count'] < 1 or end > view.get('byteOffset', 0) + view['byteLength']:
            raise ValueError('Accessor exceeds bufferView')
        return np.ndarray((a['count'], width), dtype=dtype, buffer=self.data,
                          offset=self.offset + offset, strides=(stride, dtype.itemsize))

    def image_hashes(self):
        result = []
        for image in self.doc.get('images', []):
            view = self.doc['bufferViews'][image['bufferView']]
            start = self.offset + view.get('byteOffset', 0)
            result.append(hashlib.sha256(memoryview(self.data)[start:start + view['byteLength']]).hexdigest())
        return result


class Writer:
    def __init__(self, source, output):
        self.output = Path(output)
        self.partial = self.output.with_suffix('.bin.partial')
        if self.output.exists() or self.partial.exists():
            raise ValueError('Candidate/partial already exists; choose a fresh output')
        self.output.parent.mkdir(parents=True, exist_ok=True)
        self.stream = self.partial.open('xb')
        self.doc = copy.deepcopy(source.doc)
        self.doc['accessors'], self.doc['bufferViews'] = [], []
        for image in self.doc.get('images', []):
            view = source.doc['bufferViews'][image['bufferView']]
            start = source.offset + view.get('byteOffset', 0)
            image['bufferView'] = self.write_view(memoryview(source.data)[start:start + view['byteLength']])

    def write_view(self, data):
        self.stream.write(b'\0' * (-self.stream.tell() % 4))
        start = self.stream.tell()
        self.stream.write(data)
        self.doc['bufferViews'].append({'buffer': 0, 'byteOffset': start, 'byteLength': len(data)})
        return len(self.doc['bufferViews']) - 1

    def accessor(self, values, template=None, position=False):
        values = np.ascontiguousarray(values)
        view = self.write_view(memoryview(values).cast('B'))
        width = values.shape[1] if values.ndim == 2 else 1
        if template is None:
            component = 5126 if values.dtype.kind == 'f' else 5125
            template = {'componentType': component, 'type': next(k for k, v in WIDTHS.items() if v == width)}
        result = {k: copy.deepcopy(v) for k, v in template.items()
                  if k not in ('bufferView', 'byteOffset', 'min', 'max', 'count', 'sparse')}
        result.update(bufferView=view, count=len(values))
        if position:
            result.update(min=values.min(axis=0).tolist(), max=values.max(axis=0).tolist())
        self.doc['accessors'].append(result)
        return len(self.doc['accessors']) - 1

    def finish(self):
        self.stream.write(b'\0' * (-self.stream.tell() % 4))
        length = self.stream.tell()
        self.stream.close()
        self.doc['buffers'] = [{'byteLength': length}]
        encoded = json.dumps(self.doc, separators=(',', ':')).encode()
        encoded += b' ' * (-len(encoded) % 4)
        size = 28 + len(encoded) + length
        if size > 512 * 1024 * 1024:
            raise ValueError('Candidate exceeds production 512 MiB map bound')
        with self.output.open('xb') as target, self.partial.open('rb') as binary:
            target.write(struct.pack('<5I', 0x46546c67, 2, size, len(encoded), 0x4e4f534a))
            target.write(encoded)
            target.write(struct.pack('<II', length, 0x004e4942))
            while chunk := binary.read(1024 * 1024):
                target.write(chunk)
        self.partial.unlink()
