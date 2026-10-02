"""Bounded import resources and glTF storage shared by the USD cook tool."""
import hashlib
import json
import math
from pathlib import Path
import struct


class ImportFailure(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ImportFailure(message)


class Budget:
    def __init__(self, max_bytes=256 * 1024 * 1024, max_vertices=2_000_000,
                 max_instances=1_000_000, cancel_file=None):
        self.max_bytes, self.max_vertices = max_bytes, max_vertices
        self.max_instances, self.cancel_file = max_instances, cancel_file
        self.bytes = self.vertices = self.instances = 0

    def check(self):
        if self.cancel_file and Path(self.cancel_file).exists():
            raise ImportFailure("USD import canceled")

    def charge(self, byte_count=0, vertices=0, instances=0):
        self.check()
        self.bytes += byte_count
        self.vertices += vertices
        self.instances += instances
        require(self.bytes <= self.max_bytes, "USD cooked-resource byte budget exceeded")
        require(self.vertices <= self.max_vertices, "USD cooked-vertex budget exceeded")
        require(self.instances <= self.max_instances, "USD scene-instance budget exceeded")


def digest(path, budget=None):
    hasher = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while block := stream.read(1024 * 1024):
            if budget:
                budget.check()
            hasher.update(block)
    return hasher.hexdigest()


def vector(value, size, label):
    require(value is not None and len(value) == size, f"Invalid {label}")
    result = tuple(float(component) for component in value)
    require(all(math.isfinite(c) for c in result), f"Non-finite {label}")
    return result


def normalized(value):
    result = vector(value, 3, "normal")
    length = math.sqrt(sum(c * c for c in result))
    require(length > 1e-12, "Zero-length USD normal")
    return tuple(c / length for c in result)


def matrix(value):
    # Gf uses row vectors; flattening its rows is the equivalent glTF column matrix.
    result = [float(value[row][column]) for row in range(4) for column in range(4)]
    require(all(math.isfinite(c) for c in result), "Non-finite USD transform")
    require(abs(value.GetDeterminant()) > 1e-18, "Singular USD transform")
    require(all(abs(result[i]) < 1e-10 for i in (3, 7, 11)) and abs(result[15] - 1) < 1e-10,
            "Perspective USD transforms are unsupported")
    return result


class Gltf:
    def __init__(self, directory, budget):
        self.directory, self.budget = Path(directory), budget
        self.binary = bytearray()
        self.source_images = {}
        self.metadata_reservation = 0
        self.document = {"asset": {"version": "2.0", "generator": "Octaryn OpenUSD cook"},
                         "scene": 0, "scenes": [{"nodes": []}], "nodes": [], "meshes": [],
                         "materials": [], "bufferViews": [], "accessors": []}

    def attribute(self, values, kind, positions=False, indices=False):
        components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[kind]
        self.budget.charge(len(values) * components * 4)
        offset = len(self.binary)
        packer = struct.Struct("<" + ("I" if indices else "f") * components)
        for i, row in enumerate(values):
            if i % 4096 == 0:
                self.budget.check()
            self.binary.extend(packer.pack(*(row if components != 1 else (row,))))
        view = len(self.document["bufferViews"])
        self.document["bufferViews"].append({"buffer": 0, "byteOffset": offset,
                                             "byteLength": len(self.binary) - offset})
        entry = {"bufferView": view, "componentType": 5125 if indices else 5126,
                 "count": len(values), "type": kind}
        if positions:
            entry["min"] = [min(row[axis] for row in values) for axis in range(3)]
            entry["max"] = [max(row[axis] for row in values) for axis in range(3)]
        self.document["accessors"].append(entry)
        return len(self.document["accessors"]) - 1

    def node(self, mesh, transform, name, provenance):
        self.budget.charge(instances=1)
        node = {"mesh": mesh, "matrix": matrix(transform), "name": name, "extras": {"openusd": provenance}}
        # Reserve metadata as nodes arrive, before a large final JSON string can be allocated.
        reservation = len(json.dumps(node,indent=2,allow_nan=False).encode("utf-8")) + 256
        self.budget.charge(reservation)
        self.metadata_reservation += reservation
        self.document["nodes"].append(node)
        return len(self.document["nodes"]) - 1
