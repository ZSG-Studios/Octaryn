"""USD composition and instance-preserving static scene cooking."""
from pxr import Usd, UsdGeom, Sdf
from usd_support import Gltf, require
from usd_geometry import convert
from usd_materials import Materials


def open_stage(source, populations=()):
    if populations:
        mask = Usd.StagePopulationMask()
        for path in populations:
            sdf_path = Sdf.Path(path)
            require(sdf_path.IsAbsolutePath() and sdf_path.IsPrimPath(), "Population paths must be absolute prim paths")
            mask.Add(sdf_path)
        stage = Usd.Stage.OpenMasked(str(source), mask, Usd.Stage.LoadNone)
    else:
        stage = Usd.Stage.Open(str(source), Usd.Stage.LoadNone)
    require(stage, "OpenUSD could not compose the stage")
    require(not stage.GetCompositionErrors(), "OpenUSD stage has unresolved composition arcs")
    return stage


def metadata(stage):
    return {"usdVersion":".".join(map(str, Usd.GetVersion()[1:])),
            "metersPerUnit":UsdGeom.GetStageMetersPerUnit(stage),
            "upAxis":str(UsdGeom.GetStageUpAxis(stage)),
            "defaultPrim":str(stage.GetDefaultPrim().GetPath()) if stage.GetDefaultPrim() else "",
            "loadPolicy":"none", "loadedPayloads":[str(p) for p in stage.GetLoadSet()],
            "loadablePayloads":[str(p) for p in sorted(stage.FindLoadable())],
            "population":[str(p) for p in stage.GetPopulationMask().GetPaths()],
            "composedPrims":sum(1 for _ in stage.Traverse()),
            "layers":[layer.realPath for layer in stage.GetUsedLayers() if layer.realPath]}


def cook_stage(stage, directory, budget, payloads="all", time_value=None, purposes=("default","render"), capture_sources=None):
    budget.check()
    require(payloads in ("all","none"), "Invalid USD payload policy")
    if payloads == "all":
        stage.Load()
    else:
        require(not stage.FindLoadable(), "Cook cannot discard payloads; select all or mask a payload-free population")
    require(not stage.GetCompositionErrors(), "Loaded USD stage has unresolved composition arcs")
    if capture_sources: capture_sources(stage)
    time = Usd.TimeCode.Default() if time_value is None else Usd.TimeCode(time_value)
    cache = UsdGeom.XformCache(time)
    gltf = Gltf(directory, budget)
    materials = Materials(gltf, time)
    geometry, wrappers, triangles = {}, {}, {}
    prims = []
    prim_limit = min(1_000_000,budget.max_instances * 4,budget.max_bytes // 256)
    for prim in Usd.PrimRange.Stage(stage, Usd.TraverseInstanceProxies()):
        budget.check()
        require(len(prims) < prim_limit, "USD composed prim count exceeds scene budget")
        prims.append(prim)
    point_prims = [p for p in prims if p.IsA(UsdGeom.PointInstancer)]
    prototypes = {path for p in point_prims for path in UsdGeom.PointInstancer(p).GetPrototypesRel().GetTargets()}
    instance_count, instanced_triangles = 0, 0
    native_instances = {str(p.GetPath()) for p in prims if p.IsInstance()}

    def visible(prim):
        image = UsdGeom.Imageable(prim)
        return not image or (image.ComputeVisibility(time) != "invisible" and str(image.ComputePurpose()) in purposes)

    def static(prim):
        if time_value is None:
            require(not any(attr.ValueMightBeTimeVarying() for attr in prim.GetAttributes()),
                    f"Time-sampled USD data requires an explicit --time snapshot: {prim.GetPath()}")

    def mesh_node(prim, world, provenance):
        nonlocal instance_count, instanced_triangles
        budget.check(); static(prim)
        original = prim.GetPrimInPrototype() if prim.IsInstanceProxy() else prim
        # Root instance primvars can differ even when prototype topology is shared.
        inherited = []
        api = UsdGeom.PrimvarsAPI(prim)
        for name in ("normals","st","displayColor","displayOpacity"):
            var = api.FindPrimvarWithInheritance(name)
            if var and var.HasValue():
                owner = var.GetAttr().GetPrim()
                owner = owner.GetPrimInPrototype() if owner.IsInstanceProxy() else owner
                inherited.append((name,str(owner.GetPath())))
        geometry_key = (str(original.GetPath()),tuple(inherited))
        if geometry_key not in geometry:
            geometry[geometry_key], triangles[geometry_key] = convert(prim, time, gltf)
        primitive = geometry[geometry_key]
        material = materials.material(prim, UsdGeom.Mesh(prim).GetDoubleSidedAttr().Get(time),
                                      "COLOR_0" in primitive["attributes"],
                                      primitive["extras"]["openusd"]["displayOpacityTransparent"])
        require("TEXCOORD_0" in primitive["attributes"] or not any("Texture" in name for name in gltf.document["materials"][material]),
                "Textured USD mesh is missing st UVs")
        pbr = gltf.document["materials"][material].get("pbrMetallicRoughness", {})
        require("TEXCOORD_0" in primitive["attributes"] or "baseColorTexture" not in pbr,
                "Textured USD mesh is missing st UVs")
        key = geometry_key, material
        if key not in wrappers:
            wrappers[key] = len(gltf.document["meshes"])
            mapped = dict(primitive, attributes=dict(primitive["attributes"]), material=material)
            # PreviewSurface does not implicitly read displayColor: keep unconsumed data without tinting it.
            if "material" in gltf.document["materials"][material].get("extras",{}).get("openusd",{}) and "COLOR_0" in mapped["attributes"]:
                mapped["attributes"]["_USD_DISPLAY_COLOR"] = mapped["attributes"].pop("COLOR_0")
            gltf.document["meshes"].append({"name":str(original.GetPath()),
                "primitives":[mapped]})
        node = gltf.node(wrappers[key], world, str(prim.GetPath()), provenance)
        instance_count += 1; instanced_triangles += triangles[geometry_key]
        return node

    children = []
    supported = {"", "Xform", "Scope", "Mesh", "PointInstancer", "Material", "Shader", "NodeGraph"}
    for prim in prims:
        budget.check()
        if any(prim.GetPath().HasPrefix(path) for path in prototypes):
            continue
        static(prim)
        if not visible(prim):
            continue
        require(str(prim.GetTypeName()) in supported, f"Unsupported visible USD prim type: {prim.GetTypeName()} at {prim.GetPath()}")
        if prim.IsA(UsdGeom.Mesh):
            children.append(mesh_node(prim, cache.GetLocalToWorldTransform(prim),
                {"prim":str(prim.GetPath()), "nativeInstanceProxy":prim.IsInstanceProxy()}))
        elif prim.IsA(UsdGeom.PointInstancer):
            instancer = UsdGeom.PointInstancer(prim)
            require(not any(var.GetAttr().HasAuthoredValueOpinion() for var in UsdGeom.PrimvarsAPI(prim).GetPrimvars()),
                    "PointInstancer per-instance primvars require a dedicated material representation")
            proto_paths = instancer.GetPrototypesRel().GetTargets()
            proto_indices = instancer.GetProtoIndicesAttr().Get(time)
            require(proto_indices is not None and proto_paths, "PointInstancer prototype data is missing")
            require(len(proto_indices) <= budget.max_instances, "PointInstancer exceeds scene-instance budget")
            ids = instancer.GetIdsAttr().Get(time)
            require(not ids or len(ids) == len(proto_indices), "PointInstancer id count differs from instances")
            mask = instancer.ComputeMaskAtTime(time)
            transforms = instancer.ComputeInstanceTransformsAtTime(time, time, UsdGeom.PointInstancer.ExcludeProtoXform,
                                                                  UsdGeom.PointInstancer.IgnoreMask)
            require(len(transforms) == len(proto_indices), "PointInstancer transform count mismatch")
            require(not mask or len(mask) == len(proto_indices), "PointInstancer visibility mask count mismatch")
            instancer_world = cache.GetLocalToWorldTransform(prim)
            for index, prototype_index in enumerate(proto_indices):
                budget.check()
                require(0 <= prototype_index < len(proto_paths), "PointInstancer prototype index out of range")
                if mask and not mask[index]: continue
                root = stage.GetPrimAtPath(proto_paths[prototype_index])
                require(root, "PointInstancer prototype is outside population or unresolved")
                parent = root.GetParent()
                parent_world = cache.GetLocalToWorldTransform(parent)
                relative_parent = parent_world.GetInverse()
                found = False
                for proto_prim in Usd.PrimRange(root, Usd.TraverseInstanceProxies()):
                    require(str(proto_prim.GetTypeName()) in supported and not proto_prim.IsA(UsdGeom.PointInstancer),
                            "Nested PointInstancer or unsupported prototype prim")
                    static(proto_prim)
                    if proto_prim.IsA(UsdGeom.Mesh):
                        found = True
                        if not visible(proto_prim): continue
                        # Prototype subtree is relative to its parent, then point and instancer transforms.
                        relative = cache.GetLocalToWorldTransform(proto_prim) * relative_parent
                        world = relative * transforms[index] * instancer_world
                        children.append(mesh_node(proto_prim, world,
                            {"prim":str(prim.GetPath()), "prototype":str(root.GetPath()),
                             "pointInstanceId":int(ids[index]) if ids else index}))
                require(found, "PointInstancer prototype contains no supported mesh")
    require(children, "USD selected population has no supported visible geometry")
    units = float(UsdGeom.GetStageMetersPerUnit(stage))
    require(units > 0, "USD stage metersPerUnit must be positive")
    axis = str(UsdGeom.GetStageUpAxis(stage))
    require(axis in ("Y","Z"), "USD stage up axis must be Y or Z")
    root_matrix = ([units,0,0,0, 0,units,0,0, 0,0,units,0, 0,0,0,1] if axis == "Y" else
                   [units,0,0,0, 0,0,-units,0, 0,units,0,0, 0,0,0,1])
    gltf.document["scenes"][0]["nodes"] = [len(gltf.document["nodes"])]
    gltf.document["nodes"].append({"name":"USD units/up-axis", "matrix":root_matrix, "children":children})
    gltf.document["buffers"] = [{"uri":"scene.bin", "byteLength":len(gltf.binary)}]
    gltf.document["extras"] = {"openusd":{"nativeInstances":sorted(native_instances), "staticSnapshot":time_value,
                                            "population":[str(p) for p in stage.GetPopulationMask().GetPaths()]}}
    unique_triangles = sum(triangles[key[0]] for key in wrappers)
    return gltf, {"meshes":len(gltf.document["meshes"]), "uniqueTriangles":unique_triangles,
                  "instances":instance_count, "instancedTriangles":instanced_triangles,
                  "nativeInstances":len(native_instances)}, {"unitsPerMetre":1/units, "upAxis":axis,
                  "timeCode":time_value, "payloadPolicy":payloads, "purposes":list(purposes)}
