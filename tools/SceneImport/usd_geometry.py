"""Composed static USD mesh attributes to reusable glTF geometry."""
import math
from pxr import UsdGeom
from usd_support import require, vector, normalized


def samples(primvar, time, label):
    if not primvar or not primvar.HasValue():
        return None
    require(not time.IsDefault() or (not primvar.GetAttr().ValueMightBeTimeVarying()
            and not primvar.GetIndicesAttr().ValueMightBeTimeVarying()),
            f"Time-sampled inherited primvar {label} requires an explicit --time snapshot")
    values = primvar.ComputeFlattened(time)
    require(values is not None, f"Invalid indexed primvar {label}")
    return list(values), str(primvar.GetInterpolation())


def pick(sample, vertex, face, corner, counts, label):
    if sample is None:
        return None
    values, interpolation = sample
    require(interpolation in ("constant","uniform","vertex","varying","faceVarying"),
            f"Unsupported {label} interpolation: {interpolation}")
    index = {"constant": 0, "uniform": face, "vertex": vertex,
             "varying": vertex, "faceVarying": corner}[interpolation]
    require(index < len(values), f"Invalid {label} attribute cardinality")
    return values[index]


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def subtract(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def triangulate(vertices):
    require(len(vertices) >= 3, "USD face has fewer than three vertices")
    require(len(vertices) <= 256, "USD polygon exceeds bounded triangulation limit")
    normal = cross(subtract(vertices[1], vertices[0]), subtract(vertices[2], vertices[0]))
    normal = normalized(normal)
    scale = max(1, max(math.sqrt(sum(c*c for c in subtract(p, vertices[0]))) for p in vertices))
    for point in vertices:
        require(abs(sum(normal[i]*(point[i]-vertices[0][i]) for i in range(3))) < scale * 1e-6,
                "Non-planar USD polygons require an authored triangulation")
    for i in range(len(vertices)):
        edge_a = subtract(vertices[(i+1) % len(vertices)], vertices[i])
        edge_b = subtract(vertices[(i+2) % len(vertices)], vertices[(i+1) % len(vertices)])
        require(sum(a*b for a,b in zip(cross(edge_a, edge_b), normal)) > 1e-12,
                "Concave, collinear or degenerate USD polygons require authored triangulation")
    axis = max(range(3), key=lambda i:abs(normal[i]))
    projected = [tuple(p[i] for i in range(3) if i != axis) for p in vertices]
    def orient(a,b,c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    for i in range(len(vertices)):
        a,b = projected[i], projected[(i+1)%len(vertices)]
        for j in range(i+2,len(vertices)):
            if i == 0 and j == len(vertices)-1: continue
            c,d = projected[j], projected[(j+1)%len(vertices)]
            require(not (orient(a,b,c)*orient(a,b,d) <= 0 and orient(c,d,a)*orient(c,d,b) <= 0),
                    "Self-intersecting USD polygon requires authored triangulation")
    return [(0, i, i+1) for i in range(1, len(vertices)-1)], normal


def convert(prim, time, gltf):
    mesh = UsdGeom.Mesh(prim)
    require(str(mesh.GetSubdivisionSchemeAttr().Get(time)) == "none",
            f"Subdivision mesh requires an explicit tessellation cook: {prim.GetPath()}")
    require(not mesh.GetHoleIndicesAttr().Get(time), "USD mesh holes are unsupported")
    require(not UsdGeom.Subset.GetAllGeomSubsets(mesh), "USD material/topology subsets are unsupported")
    points = mesh.GetPointsAttr().Get(time)
    counts = mesh.GetFaceVertexCountsAttr().Get(time)
    indices = mesh.GetFaceVertexIndicesAttr().Get(time)
    require(points is not None and counts is not None and indices is not None, "USD mesh topology is missing")
    require(len(points) <= gltf.budget.max_vertices and len(indices) <= gltf.budget.max_vertices * 3,
            "USD source topology exceeds vertex/corner budget")
    points = [vector(p, 3, "position") for p in points]
    counts, indices = list(counts), list(indices)
    require(points and counts and sum(counts) == len(indices), "Invalid USD face topology")
    require(all(0 <= index < len(points) for index in indices), "USD face index out of range")
    normals = mesh.GetNormalsAttr().Get(time)
    normal_sample = (list(normals), str(mesh.GetNormalsInterpolation())) if normals else None
    api = UsdGeom.PrimvarsAPI(prim)
    primvar_normals = samples(api.FindPrimvarWithInheritance("normals"), time, "normals")
    normal_sample = primvar_normals or normal_sample
    uv = samples(api.FindPrimvarWithInheritance("st"), time, "st")
    color = samples(api.FindPrimvarWithInheritance("displayColor"), time, "displayColor")
    opacity = samples(api.FindPrimvarWithInheritance("displayOpacity"), time, "displayOpacity")
    for name, sample in (("normal", normal_sample), ("st", uv), ("displayColor", color), ("displayOpacity", opacity)):
        if sample:
            expected = {"constant":1, "uniform":len(counts), "vertex":len(points),
                        "varying":len(points), "faceVarying":len(indices)}.get(sample[1])
            require(expected is not None and len(sample[0]) == expected, f"Invalid {name} cardinality/interpolation")
    positions, output_normals, uvs, colors, output_indices, lookup = [], [], [], [], [], {}
    corner = 0
    reverse = str(mesh.GetOrientationAttr().Get(time)) == "leftHanded"
    for face, count in enumerate(counts):
        gltf.budget.check()
        face_indices = indices[corner:corner+count]
        triangles, face_normal = triangulate([points[v] for v in face_indices])
        if reverse:
            face_normal = tuple(-c for c in face_normal)
        for tri in triangles:
            if reverse:
                tri = (tri[0], tri[2], tri[1])
            for local in tri:
                v, c = face_indices[local], corner + local
                normal = pick(normal_sample, v, face, c, counts, "normal")
                n = normalized(normal) if normal is not None else face_normal
                st = pick(uv, v, face, c, counts, "st")
                st = vector(st, 2, "UV") if st is not None else None
                # USD texture origin is lower-left; glTF image origin is upper-left.
                st = (st[0], 1-st[1]) if st is not None else None
                rgb = pick(color, v, face, c, counts, "displayColor")
                rgb = vector(rgb, 3, "displayColor") if rgb is not None else (1,1,1)
                alpha = pick(opacity, v, face, c, counts, "displayOpacity")
                rgba = rgb + (float(alpha) if alpha is not None else 1,)
                require(all(math.isfinite(x) and 0 <= x <= 1 for x in rgba), "Invalid USD display color/opacity")
                key = (v, n, st, rgba)
                if key not in lookup:
                    gltf.budget.charge(vertices=1)
                    lookup[key] = len(positions)
                    positions.append(points[v]); output_normals.append(n)
                    if uv: uvs.append(st)
                    if color or opacity: colors.append(rgba)
                output_indices.append(lookup[key])
        corner += count
    attributes = {"POSITION": gltf.attribute(positions, "VEC3", positions=True),
                  "NORMAL": gltf.attribute(output_normals, "VEC3")}
    if uv: attributes["TEXCOORD_0"] = gltf.attribute(uvs, "VEC2")
    if color or opacity: attributes["COLOR_0"] = gltf.attribute(colors, "VEC4")
    return {"attributes": attributes, "indices": gltf.attribute(output_indices, "SCALAR", indices=True),
            "extras":{"openusd":{"displayOpacityPresent":bool(opacity),
                "displayOpacityTransparent":any(rgba[3] < 1 for rgba in colors)}}}, len(output_indices)//3
