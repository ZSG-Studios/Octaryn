"""Authored composition/instance fixtures; no proprietary content or renderer needed."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from usd_import import setup_sdk, cook, inspect
setup_sdk()
from pxr import Usd, UsdGeom, UsdShade, Sdf
from usd_support import Budget, ImportFailure


def mesh(stage, path):
    result = UsdGeom.Mesh.Define(stage, path)
    result.CreateSubdivisionSchemeAttr("none")
    result.CreatePointsAttr([(0,0,0),(1,0,0),(0,1,0)])
    result.CreateFaceVertexCountsAttr([3]); result.CreateFaceVertexIndicesAttr([0,1,2])
    result.CreateNormalsAttr([(0,0,1)]*3); result.SetNormalsInterpolation("vertex")
    api = UsdGeom.PrimvarsAPI(result)
    st = api.CreatePrimvar("st", Sdf.ValueTypeNames.TexCoord2fArray, "faceVarying")
    st.Set([(0,0),(1,0),(0,1)]); st.SetIndices([0,1,2])
    color = api.CreatePrimvar("displayColor", Sdf.ValueTypeNames.Color3fArray, "constant")
    color.Set([(0.2,0.4,0.6)])
    return result


def fixture(directory):
    prototype = Usd.Stage.CreateNew(str(directory / "asset.usda"))
    root = UsdGeom.Xform.Define(prototype,"/Asset")
    prototype.SetDefaultPrim(root.GetPrim()); mesh(prototype,"/Asset/Mesh")
    prototype.GetRootLayer().Save()
    stage = Usd.Stage.CreateNew(str(directory / "source.usda"))
    UsdGeom.SetStageMetersPerUnit(stage,0.01); UsdGeom.SetStageUpAxis(stage,"Z")
    world = UsdGeom.Xform.Define(stage,"/World"); stage.SetDefaultPrim(world.GetPrim())
    for name, x in (("A",10),("B",20)):
        node = UsdGeom.Xform.Define(stage,"/World/"+name)
        node.GetPrim().GetReferences().AddReference("asset.usda")
        node.GetPrim().SetInstanceable(True)
        node.AddTranslateOp().Set((x,0,0))
    points = UsdGeom.PointInstancer.Define(stage,"/World/Instances")
    points.AddTranslateOp().Set((0,10,0))
    point_prototype = UsdGeom.Xform.Define(stage,"/World/Instances/Prototype")
    point_prototype.GetPrim().GetReferences().AddReference("asset.usda")
    point_prototype.AddTranslateOp().Set((1,0,0))
    points.GetPrototypesRel().SetTargets([point_prototype.GetPath()])
    points.CreateProtoIndicesAttr([0,0,0]); points.CreatePositionsAttr([(30,0,0),(40,0,0),(50,0,0)])
    points.CreateScalesAttr([(2,2,2),(1,1,1),(1,1,1)])
    points.CreateIdsAttr([1001,1002,1003]); points.CreateInvisibleIdsAttr([1002])
    payload = UsdGeom.Xform.Define(stage,"/World/Payload")
    payload.GetPrim().GetPayloads().AddPayload("asset.usda")
    payload.AddTranslateOp().Set((60,0,0))
    stage.GetRootLayer().Save()
    stage.GetRootLayer().Export(str(directory / "source.usdc"))
    return directory / "source.usda"


def load_package(output):
    scene = json.loads((output / "scene.gltf").read_text())
    descriptor = json.loads((output / "scene-import.json").read_text())
    return scene, descriptor, (output / "scene.bin").read_bytes()


def values(scene, binary, index):
    accessor = scene["accessors"][index]; view = scene["bufferViews"][accessor["bufferView"]]
    width = {"VEC2":2,"VEC3":3,"VEC4":4,"SCALAR":1}[accessor["type"]]
    fmt = "<" + ("I" if accessor["componentType"] == 5125 else "f") * width
    return list(struct.iter_unpack(fmt,binary[view["byteOffset"]:view["byteOffset"]+view["byteLength"]]))


class UsdImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="octaryn-usd-tests-")
        self.directory = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_composition_instances_payload_units_and_binary_layer(self):
        source = fixture(self.directory)
        metadata = inspect(source)
        self.assertEqual(metadata["loadedPayloads"],[])
        self.assertIn("/World/Payload",metadata["loadablePayloads"])
        self.assertEqual(metadata["upAxis"],"Z")
        self.assertEqual(metadata["metersPerUnit"],0.01)
        for suffix in ("usda","usdc"):
            output = self.directory / suffix
            result = cook(source.with_suffix("."+suffix),output)
            self.assertEqual(result["counts"]["nativeInstances"],2)
            self.assertEqual(result["counts"]["instances"],5)
            self.assertEqual(result["counts"]["instancedTriangles"],5)
            scene, descriptor, binary = load_package(output)
            self.assertLess(len(scene["meshes"]),5)
            for file in descriptor["files"]:
                data = (output / file["path"]).read_bytes()
                self.assertEqual(len(data),file["bytes"])
                self.assertEqual(hashlib.sha256(data).hexdigest(),file["sha256"])
            geometry_nodes = [node for node in scene["nodes"] if "mesh" in node]
            self.assertEqual(geometry_nodes[0]["mesh"],geometry_nodes[1]["mesh"])
            point_nodes = [node for node in geometry_nodes if "pointInstanceId" in node["extras"]["openusd"]]
            self.assertEqual([node["extras"]["openusd"]["pointInstanceId"] for node in point_nodes],[1001,1003])
            self.assertEqual(point_nodes[0]["mesh"],point_nodes[1]["mesh"])
            self.assertAlmostEqual(point_nodes[0]["matrix"][12],32)
            self.assertAlmostEqual(point_nodes[0]["matrix"][13],10)
            self.assertAlmostEqual(point_nodes[1]["matrix"][12],51)
            root = scene["nodes"][-1]["matrix"]
            self.assertEqual(root[9],0.01); self.assertEqual(root[6],-0.01)
            primitive = scene["meshes"][0]["primitives"][0]
            normals = values(scene,binary,primitive["attributes"]["NORMAL"])
            self.assertEqual(normals[0],(0,0,1))
            uv = values(scene,binary,primitive["attributes"]["TEXCOORD_0"])
            self.assertEqual(uv,[(0,1),(1,1),(0,0)])
            colors = values(scene,binary,primitive["attributes"]["COLOR_0"])
            self.assertAlmostEqual(colors[0][1],0.4)
            self.assertEqual(scene["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"],[1,1,1,1])
        with self.assertRaisesRegex(ImportFailure,"discard payloads"):
            cook(source,self.directory/"no-payload",payloads="none")
        self.assertFalse((self.directory/"no-payload").exists())

    def test_population_limits_cancel_and_snapshot(self):
        source = fixture(self.directory)
        result = cook(source,self.directory/"masked",populations=["/World/A"],payloads="none")
        self.assertEqual(result["counts"]["instances"],1)
        for budget in (Budget(max_bytes=8), Budget(max_vertices=2), Budget(max_instances=1)):
            with self.assertRaises(ImportFailure): cook(source,self.directory/"bounded",budget=budget)
            self.assertFalse((self.directory/"bounded").exists())
        cancel = self.directory/"cancel"; cancel.touch()
        with self.assertRaisesRegex(ImportFailure,"canceled"):
            cook(source,self.directory/"canceled",budget=Budget(cancel_file=cancel))
        self.assertFalse((self.directory/"canceled").exists())
        stage = Usd.Stage.CreateNew(str(self.directory/"animated.usda"))
        shape = mesh(stage,"/Mesh"); shape.AddTranslateOp().Set((5,0,0),1)
        shape.GetPointsAttr().Set([(0,0,0),(1,0,0),(0,1,0)],1)
        shape.GetPointsAttr().Set([(0,0,0),(2,0,0),(0,2,0)],2)
        stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"snapshot"):
            cook(self.directory/"animated.usda",self.directory/"animated-default")
        result = cook(self.directory/"animated.usda",self.directory/"snapshot",time_value=2)
        self.assertEqual(result["timeCode"],2)
        with self.assertRaisesRegex(ImportFailure,"finite"):
            cook(self.directory/"animated.usda",self.directory/"nan-snapshot",time_value=float("nan"))
        self.assertFalse((self.directory/"nan-snapshot").exists())

    def test_unsupported_mesh_and_shader_fail_without_output(self):
        stage = Usd.Stage.CreateNew(str(self.directory/"bad.usda"))
        shape = mesh(stage,"/Mesh")
        shape.GetSubdivisionSchemeAttr().Set("catmullClark"); stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"Subdivision"):
            cook(self.directory/"bad.usda",self.directory/"out")
        shape.GetSubdivisionSchemeAttr().Set("none")
        material = UsdShade.Material.Define(stage,"/Material")
        shader = UsdShade.Shader.Define(stage,"/Material/Shader"); shader.CreateIdAttr("UnsupportedShader")
        material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(),"surface")
        UsdShade.MaterialBindingAPI.Apply(shape.GetPrim()).Bind(material); stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"UsdPreviewSurface"):
            cook(self.directory/"bad.usda",self.directory/"out")
        self.assertFalse((self.directory/"out").exists())

    def test_preview_surface_texture_and_unsupported_connections(self):
        def chunk(name, data):
            return struct.pack(">I",len(data))+name+data+struct.pack(">I",zlib.crc32(name+data))
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR",struct.pack(">IIBBBBB",1,1,8,6,0,0,0))
        png += chunk(b"IDAT",zlib.compress(b"\x00\x80\x40\x20\xff")) + chunk(b"IEND",b"")
        (self.directory/"pixel.png").write_bytes(png)
        stage = Usd.Stage.CreateNew(str(self.directory/"material.usda"))
        shape = mesh(stage,"/Mesh")
        material = UsdShade.Material.Define(stage,"/Material")
        surface = UsdShade.Shader.Define(stage,"/Material/Surface")
        surface.CreateIdAttr("UsdPreviewSurface")
        surface.CreateInput("emissiveColor",Sdf.ValueTypeNames.Color3f).Set((2,1,0))
        surface.CreateInput("roughness",Sdf.ValueTypeNames.Float).Set(0.7)
        texture = UsdShade.Shader.Define(stage,"/Material/Texture"); texture.CreateIdAttr("UsdUVTexture")
        texture.CreateInput("file",Sdf.ValueTypeNames.Asset).Set(Sdf.AssetPath("pixel.png"))
        texture.CreateInput("wrapS",Sdf.ValueTypeNames.Token).Set("repeat")
        texture.CreateInput("wrapT",Sdf.ValueTypeNames.Token).Set("clamp")
        reader = UsdShade.Shader.Define(stage,"/Material/St"); reader.CreateIdAttr("UsdPrimvarReader_float2")
        reader.CreateInput("varname",Sdf.ValueTypeNames.String).Set("st")
        texture.CreateInput("st",Sdf.ValueTypeNames.Float2).ConnectToSource(reader.ConnectableAPI(),"result")
        surface.CreateInput("diffuseColor",Sdf.ValueTypeNames.Color3f).ConnectToSource(texture.ConnectableAPI(),"rgb")
        material.CreateSurfaceOutput().ConnectToSource(surface.ConnectableAPI(),"surface")
        UsdShade.MaterialBindingAPI.Apply(shape.GetPrim()).Bind(material); stage.GetRootLayer().Save()
        result = cook(self.directory/"material.usda",self.directory/"textured")
        scene, descriptor, _ = load_package(self.directory/"textured")
        self.assertEqual(scene["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"],{"index":0})
        self.assertEqual(scene["materials"][0]["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"],2)
        self.assertEqual(scene["samplers"][0]["wrapT"],33071)
        self.assertNotIn("COLOR_0",scene["meshes"][0]["primitives"][0]["attributes"])
        self.assertIn("_USD_DISPLAY_COLOR",scene["meshes"][0]["primitives"][0]["attributes"])
        self.assertEqual(len(descriptor["files"]),3)
        surface.CreateInput("normal",Sdf.ValueTypeNames.Normal3f).ConnectToSource(texture.ConnectableAPI(),"rgb")
        stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"normal texture"):
            cook(self.directory/"material.usda",self.directory/"unsupported-normal")
        self.assertFalse((self.directory/"unsupported-normal").exists())
        surface.GetPrim().RemoveProperty("inputs:normal")
        normal = surface.CreateInput("normal",Sdf.ValueTypeNames.Normal3f)
        normal.Set((0,0,1)); normal.Set((0,0,1),1); normal.Set((0,1,0),2)
        stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"snapshot"):
            cook(self.directory/"material.usda",self.directory/"animated-normal")
        cook(self.directory/"material.usda",self.directory/"normal-snapshot",time_value=1)
        surface.GetPrim().RemoveProperty("inputs:normal")
        varname = reader.GetInput("varname")
        varname.Set("st",1); varname.Set("other",2); stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"snapshot"):
            cook(self.directory/"material.usda",self.directory/"animated-reader")
        cook(self.directory/"material.usda",self.directory/"reader-snapshot",time_value=1)
        reader.GetPrim().RemoveProperty("inputs:varname")
        reader.CreateInput("varname",Sdf.ValueTypeNames.String).Set("st")
        roughness = surface.GetInput("roughness")
        roughness.Set(0.1,1); roughness.Set(0.9,2); stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"snapshot"):
            cook(self.directory/"material.usda",self.directory/"animated-material")
        cook(self.directory/"material.usda",self.directory/"material-snapshot",time_value=2)
        sampled,_,_ = load_package(self.directory/"material-snapshot")
        self.assertAlmostEqual(sampled["materials"][0]["pbrMetallicRoughness"]["roughnessFactor"],0.9)
        (self.directory/"pixel.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        with self.assertRaisesRegex(ImportFailure,"PNG"):
            cook(self.directory/"material.usda",self.directory/"truncated-image",time_value=2)
        self.assertFalse((self.directory/"truncated-image").exists())

    def test_native_instance_inherited_primvars_are_not_shared_incorrectly(self):
        source = fixture(self.directory)
        asset = Usd.Stage.Open(str(self.directory/"asset.usda"))
        asset.GetPrimAtPath("/Asset/Mesh").RemoveProperty("primvars:displayColor")
        asset.GetRootLayer().Save()
        stage = Usd.Stage.Open(str(source))
        for path,color in (("/World/A",(1,0,0)),("/World/B",(0,1,0))):
            UsdGeom.PrimvarsAPI(stage.GetPrimAtPath(path)).CreatePrimvar("displayColor",Sdf.ValueTypeNames.Color3fArray,"constant").Set([color])
        stage.GetRootLayer().Save()
        cook(source,self.directory/"different-colors",populations=["/World/A","/World/B"],payloads="none")
        scene,_,binary = load_package(self.directory/"different-colors")
        nodes = [node for node in scene["nodes"] if "mesh" in node]
        colors = []
        for node in nodes:
            primitive = scene["meshes"][node["mesh"]]["primitives"][0]
            colors.append(values(scene,binary,primitive["attributes"]["COLOR_0"])[0])
        self.assertEqual(colors,[(1,0,0,1),(0,1,0,1)])

    def test_inherited_animation_and_changed_source_do_not_publish(self):
        source = fixture(self.directory)
        class ChangingBudget(Budget):
            changed = False
            def charge(self,byte_count=0,vertices=0,instances=0):
                super().charge(byte_count,vertices,instances)
                if vertices and not self.changed:
                    source.write_text(source.read_text()+"\n# external change while cooking\n")
                    self.changed = True
        with self.assertRaisesRegex(ImportFailure,"changed"):
            cook(source,self.directory/"changed",budget=ChangingBudget())
        self.assertFalse((self.directory/"changed").exists())
        stage = Usd.Stage.CreateNew(str(self.directory/"inherited.usda"))
        scope = UsdGeom.Scope.Define(stage,"/World")
        shape = mesh(stage,"/World/Mesh")
        shape.GetPrim().RemoveProperty("primvars:st")
        shape.GetPrim().RemoveProperty("primvars:st:indices")
        st = UsdGeom.PrimvarsAPI(scope).CreatePrimvar("st",Sdf.ValueTypeNames.TexCoord2fArray,"constant")
        st.Set([(0,0)],1); st.Set([(1,1)],2); stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ImportFailure,"snapshot"):
            cook(self.directory/"inherited.usda",self.directory/"default-inherited")
        cook(self.directory/"inherited.usda",self.directory/"sampled-inherited",time_value=2)
        scene,_,binary = load_package(self.directory/"sampled-inherited")
        uv = values(scene,binary,scene["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"])
        self.assertEqual(uv,[(1,0)]*3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
