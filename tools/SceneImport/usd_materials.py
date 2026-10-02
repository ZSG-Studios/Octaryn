"""Explicit UsdPreviewSurface subset; unsupported networks fail the cook."""
from pathlib import Path
from pxr import UsdShade
from usd_support import require, vector, digest
from usd_images import validate


class Materials:
    def __init__(self, gltf, time):
        self.gltf, self.time = gltf, time
        self.cache, self.images, self.textures = {}, {}, {}

    def scalar(self, shader, name, default):
        source = shader.GetInput(name)
        require(not source or not self.time.IsDefault() or not source.GetAttr().ValueMightBeTimeVarying(),
                f"Time-sampled USD shader input {name} requires an explicit --time snapshot")
        require(not source or not source.HasConnectedSource(), f"Unsupported connected UsdPreviewSurface input: {name}")
        value = source.Get(self.time) if source else None
        return default if value is None else float(value)

    def connection(self, source):
        connected = source.GetConnectedSource()
        require(connected, "Unresolved USD shader connection")
        shader = UsdShade.Shader(connected[0].GetPrim())
        require(shader and str(shader.GetIdAttr().Get()) == "UsdUVTexture", "Only direct UsdUVTexture connections are supported")
        return shader, str(connected[1])

    def texture(self, source, role):
        shader, channel = self.connection(source)
        require(not self.time.IsDefault() or not any(inp.GetAttr().ValueMightBeTimeVarying() for inp in shader.GetInputs()),
                "Time-sampled USD texture inputs require an explicit --time snapshot")
        require(channel == ("a" if role == "opacity" else "rgb"), f"Unsupported USD {role} texture output")
        for name, default in (("scale", (1,1,1,1)), ("bias", (0,0,0,0))):
            inp = shader.GetInput(name)
            require(not inp or not inp.HasConnectedSource(), "Connected UV texture scale/bias unsupported")
            require(not inp or inp.Get(self.time) is None or tuple(inp.Get(self.time)) == default,
                    "Nonidentity UV texture scale/bias requires image processing")
        st = shader.GetInput("st")
        require(st and st.HasConnectedSource(), "USD texture requires an explicit st primvar reader")
        connected = st.GetConnectedSource()
        reader = UsdShade.Shader(connected[0].GetPrim()) if connected else None
        require(reader and str(reader.GetIdAttr().Get()) == "UsdPrimvarReader_float2", "USD UV transform network unsupported")
        require(not self.time.IsDefault() or not any(inp.GetAttr().ValueMightBeTimeVarying() for inp in reader.GetInputs()),
                "Time-sampled USD primvar reader requires an explicit --time snapshot")
        varname = reader.GetInput("varname")
        require(varname and not varname.HasConnectedSource() and str(varname.Get(self.time)) == "st",
                "USD textures require the supported st primvar")
        file = shader.GetInput("file")
        require(file and not file.HasConnectedSource(), "Connected USD asset paths unsupported")
        asset = file.Get(self.time)
        require(asset and asset.resolvedPath, "Unresolved USD texture asset")
        path = Path(asset.resolvedPath).resolve(strict=True)
        require(path.suffix.lower() in (".png", ".jpg", ".jpeg"), "USD texture cook currently requires PNG/JPEG")
        color = shader.GetInput("sourceColorSpace")
        require(not color or not color.HasConnectedSource(), "Connected USD texture colorspace unsupported")
        colorspace = str(color.Get(self.time)) if color and color.Get(self.time) else "auto"
        require(colorspace in ("auto", "sRGB"), "USD color texture must use sRGB/auto encoding")
        image_key = str(path)
        if image_key not in self.images:
            size = path.stat().st_size
            self.gltf.budget.charge(size)
            validate(path,self.gltf.budget)
            with path.open("rb") as stream: magic = stream.read(8)
            require(magic == b"\x89PNG\r\n\x1a\n" or magic[:3] == b"\xff\xd8\xff", "USD image signature is invalid")
            image = len(self.gltf.document.setdefault("images", []))
            source_hash = digest(path, self.gltf.budget)
            name = "textures/" + source_hash + path.suffix.lower()
            target = self.gltf.directory / name
            target.parent.mkdir(exist_ok=True)
            with path.open("rb") as src, target.open("wb") as dest:
                while block := src.read(1024 * 1024):
                    self.gltf.budget.check(); dest.write(block)
            require(digest(target,self.gltf.budget) == source_hash, "USD image changed during copying")
            validate(target,self.gltf.budget)
            self.gltf.source_images[path] = source_hash
            self.gltf.document["images"].append({"uri": name})
            self.images[image_key] = image
        wraps = []
        for name in ("wrapS", "wrapT"):
            value = shader.GetInput(name)
            require(not value or not value.HasConnectedSource(), "Connected USD wrap mode unsupported")
            token = str(value.Get(self.time)) if value and value.Get(self.time) else "useMetadata"
            require(token in ("repeat", "clamp", "mirror"), "USD texture requires explicit repeat/clamp/mirror wrapping")
            wraps.append({"repeat":10497, "clamp":33071, "mirror":33648}[token])
        key = (image_key, *wraps)
        if key not in self.textures:
            samplers = self.gltf.document.setdefault("samplers", [])
            sampler = len(samplers); samplers.append({"wrapS":wraps[0], "wrapT":wraps[1]})
            textures = self.gltf.document.setdefault("textures", [])
            self.textures[key] = len(textures)
            textures.append({"source":self.images[image_key], "sampler":sampler})
        return self.textures[key]

    def color(self, shader, name, default):
        source = shader.GetInput(name)
        require(not source or not self.time.IsDefault() or not source.GetAttr().ValueMightBeTimeVarying(),
                f"Time-sampled USD shader input {name} requires an explicit --time snapshot")
        if source and source.HasConnectedSource():
            return (1,1,1), self.texture(source, name)
        value = source.Get(self.time) if source else None
        return vector(default if value is None else value, 3, name), None

    def material(self, prim, double_sided, has_color=False, vertex_alpha=False):
        bound, relationship = UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()
        require(bound or not relationship, "USD material binding is unresolved")
        bound_prim = bound.GetPrim() if bound else None
        bound_path = bound_prim.GetPrimInPrototype().GetPath() if bound_prim and bound_prim.IsInstanceProxy() else bound.GetPath() if bound else ""
        key = (str(bound_path), double_sided, has_color if not bound else False,
               vertex_alpha if not bound else False)
        if key in self.cache: return self.cache[key]
        entry = {"doubleSided":bool(double_sided)}
        if not bound:
            entry["pbrMetallicRoughness"] = {"baseColorFactor":[1,1,1,1] if has_color else [0.18,0.18,0.18,1],
                                             "metallicFactor":0, "roughnessFactor":0.5}
            if vertex_alpha: entry["alphaMode"] = "BLEND"
        else:
            surface = bound.ComputeSurfaceSource()
            shader = surface[0] if surface else None
            require(shader and str(shader.GetIdAttr().Get()) == "UsdPreviewSurface", "Only composed UsdPreviewSurface materials are supported")
            require(not self.time.IsDefault() or not any(inp.GetAttr().ValueMightBeTimeVarying() for inp in shader.GetInputs()),
                    "Time-sampled USD shader inputs require an explicit --time snapshot")
            for name, default in (("useSpecularWorkflow",0), ("clearcoat",0), ("clearcoatRoughness",0.01),
                                  ("ior",1.5), ("displacement",0), ("occlusion",1)):
                require(self.scalar(shader,name,default) == default, f"Unsupported UsdPreviewSurface feature: {name}")
            normal = shader.GetInput("normal")
            require(not normal or not normal.HasConnectedSource(), "USD normal texture decoding/tangent convention is not yet supported")
            require(not normal or normal.Get(self.time) is None or tuple(normal.Get(self.time)) == (0,0,1), "Custom USD normal input unsupported")
            supported = {"diffuseColor","emissiveColor","metallic","roughness","opacity","opacityThreshold",
                         "useSpecularWorkflow","specularColor","clearcoat","clearcoatRoughness","ior","normal","displacement","occlusion"}
            for inp in shader.GetInputs(): require(inp.GetBaseName() in supported, f"Unknown UsdPreviewSurface input: {inp.GetBaseName()}")
            specular = shader.GetInput("specularColor")
            require(not specular or not specular.HasConnectedSource(), "Connected specular color unsupported")
            diffuse, diffuse_texture = self.color(shader, "diffuseColor", (0.18,0.18,0.18))
            emissive, emissive_texture = self.color(shader, "emissiveColor", (0,0,0))
            opacity = shader.GetInput("opacity")
            if opacity and opacity.HasConnectedSource():
                require(diffuse_texture is not None and self.texture(opacity,"opacity") == diffuse_texture,
                        "USD opacity texture must use the diffuse texture alpha")
                alpha = 1
            else: alpha = self.scalar(shader,"opacity",1)
            metallic, roughness = self.scalar(shader,"metallic",0), self.scalar(shader,"roughness",0.5)
            require(all(0 <= value <= 1 for value in (*diffuse, alpha, metallic, roughness)), "USD material factors outside glTF range")
            pbr = {"baseColorFactor":[*diffuse,alpha], "metallicFactor":metallic, "roughnessFactor":roughness}
            if diffuse_texture is not None: pbr["baseColorTexture"] = {"index":diffuse_texture}
            entry["pbrMetallicRoughness"] = pbr
            threshold = self.scalar(shader,"opacityThreshold",0)
            require(0 <= threshold <= 1, "Invalid USD opacity threshold")
            entry["alphaMode"] = "MASK" if threshold > 0 else "BLEND" if alpha < 1 or (opacity and opacity.HasConnectedSource()) else "OPAQUE"
            if threshold > 0: entry["alphaCutoff"] = threshold
            require(all(value >= 0 for value in emissive), "Negative USD emission")
            strength = max(1, *emissive)
            entry["emissiveFactor"] = [value/strength for value in emissive]
            if emissive_texture is not None: entry["emissiveTexture"] = {"index":emissive_texture}
            if strength > 1:
                entry["extensions"] = {"KHR_materials_emissive_strength":{"emissiveStrength":strength}}
                self.gltf.document["extensionsUsed"] = ["KHR_materials_emissive_strength"]
            entry["extras"] = {"openusd":{"material":str(bound.GetPath()), "mapping":"UsdPreviewSurface metallic workflow"}}
        self.cache[key] = len(self.gltf.document["materials"])
        self.gltf.document["materials"].append(entry)
        return self.cache[key]
