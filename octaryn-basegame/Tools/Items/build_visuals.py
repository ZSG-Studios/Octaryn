"""Build original basegame item meshes in metres, centered on the physics pose."""
import argparse
import json
import math
from pathlib import Path
import struct


def material(name, color, roughness=.65, metal=0, emission=None):
    result = {"name": name, "doubleSided": False, "pbrMetallicRoughness": {
        "baseColorFactor": [*color, 1], "roughnessFactor": roughness, "metallicFactor": metal}}
    if emission:
        result["emissiveFactor"] = emission
    return result


class Mesh:
    def __init__(self):
        self.parts = []

    def surface(self, rings, segments, point, mat, transform=lambda p: p):
        positions, normals, indices = [], [], []
        # Flat facets give these small first-party objects a deliberate sculpted form.
        for row in range(rings - 1):
            for column in range(segments):
                a, b = column / segments * math.tau, (column + 1) / segments * math.tau
                quad = [point(row, a), point(row + 1, a), point(row + 1, b), point(row, b)]
                for order in ((0, 1, 2), (0, 2, 3)):
                    triangle = [transform(quad[i]) for i in order]
                    u = [triangle[1][i] - triangle[0][i] for i in range(3)]
                    v = [triangle[2][i] - triangle[0][i] for i in range(3)]
                    normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
                    length = math.sqrt(sum(n*n for n in normal))
                    if length < 1e-10:
                        continue
                    normal = [n/length for n in normal]
                    indices.extend(range(len(positions), len(positions) + 3))
                    positions.extend(triangle)
                    normals.extend([normal] * 3)
        self.parts.append((positions, normals, indices, mat))

    def lathe(self, profile, segments, mat, transform=lambda p: p):
        self.surface(len(profile), segments,
                     lambda row, angle: (profile[row][1]*math.cos(angle), profile[row][0],
                                         profile[row][1]*math.sin(angle)), mat, transform)

    def sphere(self, scale, segments, rings, mat, shape=lambda p, a: p):
        def point(row, angle):
            latitude = -math.pi / 2 + row / rings * math.pi
            p = (scale[0]*math.cos(latitude)*math.cos(angle), scale[1]*math.sin(latitude),
                 scale[2]*math.cos(latitude)*math.sin(angle))
            return shape(p, angle)
        self.surface(rings + 1, segments, point, mat)

    def write(self, path):
        data, views, accessors, primitives, materials = bytearray(), [], [], [], []
        def accessor(values, kind, width, component=5126):
            while len(data) % 4:
                data.append(0)
            start = len(data)
            flat = [n for value in values for n in value] if width > 1 else values
            data.extend(struct.pack("<" + ("f" if component == 5126 else "I") * len(flat), *flat))
            views.append({"buffer": 0, "byteOffset": start, "byteLength": len(data) - start})
            record = {"bufferView": len(views)-1, "componentType": component, "count": len(values), "type": kind}
            if width == 3:
                record["min"] = [min(v[a] for v in values) for a in range(3)]
                record["max"] = [max(v[a] for v in values) for a in range(3)]
            accessors.append(record)
            return len(accessors)-1
        for positions, normals, indices, mat in self.parts:
            pos, norm = accessor(positions, "VEC3", 3), accessor(normals, "VEC3", 3)
            uv = accessor([(0., 0.)] * len(positions), "VEC2", 2)
            ind = accessor(indices, "SCALAR", 1, 5125)
            materials.append(mat)
            primitives.append({"attributes": {"POSITION": pos, "NORMAL": norm, "TEXCOORD_0": uv},
                               "indices": ind, "material": len(materials)-1})
        doc = {"asset": {"version": "2.0", "generator": "Octaryn basegame original item content v1"},
               "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
               "meshes": [{"primitives": primitives}], "materials": materials,
               "buffers": [{"byteLength": len(data)}], "bufferViews": views, "accessors": accessors}
        encoded = json.dumps(doc, separators=(",", ":")).encode()
        encoded += b" " * (-len(encoded) % 4)
        data += b"\0" * (-len(data) % 4)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(data)) +
                         struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
                         struct.pack("<II", len(data), 0x004E4942) + data)
        return {"mesh": path.name, "triangles": sum(len(p[2])//3 for p in self.parts), "bytes": path.stat().st_size}


def build(output):
    output.mkdir(parents=True, exist_ok=True)
    coin = Mesh()
    gold = material("warm gold", [.83, .53, .13], .29, .92)
    rotate = lambda p: (p[0], p[2], -p[1])
    coin.lathe([(-.016, 0), (-.016, .105), (-.010, .128), (.010, .128), (.016, .105), (.016, 0)], 32, gold, rotate)
    coin.lathe([(.016, 0), (.016, .065), (.020, .061), (.020, 0)], 20,
               material("coin relief", [.95, .7, .2], .34, .9), rotate)
    apple = Mesh()
    apple.sphere((.13, .13, .13), 24, 16, material("apple red", [.65, .032, .025], .38),
                 lambda p, a: (p[0]*(1+.07*math.cos(a*5)), p[1]*(.88+.12*abs(math.sin(a*2))), p[2]*(1+.07*math.cos(a*5))))
    apple.lathe([(.105, 0), (.105, .012), (.19, .008), (.19, 0)], 7,
                material("apple stem", [.18, .078, .026]))
    apple.sphere((.045, .006, .02), 10, 4, material("apple leaf", [.06, .24, .025]),
                 lambda p, _: (p[0]+.035, p[1]+.157+p[0]*.25, p[2]))
    torch = Mesh()
    torch.lathe([(-.14, 0), (-.14, .022), (.095, .03), (.095, 0)], 9,
                material("torch wood", [.29, .13, .045]))
    torch.lathe([(.06, 0), (.06, .044), (.11, .05), (.13, .033), (.13, 0)], 12,
                material("torch binding", [.095, .075, .043], .9))
    torch.lathe([(.10, 0), (.11, .032), (.17, .04), (.24, 0)], 12,
                material("torch ember", [1., .31, .045], .55, emission=[1., .28, .035]))
    pebble = Mesh()
    pebble.sphere((.15, .09, .115), 14, 8, material("river stone", [.28, .31, .32], .84),
                  lambda p, a: (p[0]*(1+.12*math.cos(3*a)), p[1]+.012*math.cos(a*2)*(1-abs(p[1])/.09), p[2]))
    return [mesh.write(output / f"{name}.glb") for name, mesh in
            (("coin", coin), ("apple", apple), ("torch", torch), ("pebble", pebble))]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps({"version": 1, "items": build(args.output)}, indent=2))
