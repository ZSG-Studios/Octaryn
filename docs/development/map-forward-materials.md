# Authored forward material effects

Static glTF BLEND materials may declare two bounded, generic extras:

```json
{"octaryn_blend":"additive","octaryn_view_fade":{"version":1,"parameters":[1,-0.0000000437113883,1,0]}}
```

`octaryn_blend` accepts `additive`, selecting SrcAlpha/One colour blending.
Absent declarations retain ordinary SrcAlpha/InvSrcAlpha blending. Additive and
ordinary draws retain the existing global transparent depth order and depth test.
Neither writes depth.

View fade parameters x/y lie in [-1,1], z/w in [0,1]. All must be finite, and x
must differ from y after float32 conversion. Ascending and descending endpoints
are supported. For each vertex, evaluate:

`t = saturate((abs(dot(normalize(normal),normalize(position-eye)))-x)/(y-x))`

`opacity = lerp(z,w,t*t*(3-2*t))`

The rasterizer interpolates opacity and multiplies it into the texture, vertex
and material alpha product. World-space dot products are equivalent to view-space
dot products under the camera's orthonormal rotation. The authored geometric
normal is used before normal mapping. Values are per-draw uniforms, preserving
the 1072-byte global GPU material record.

Source import rejects these declarations on OPAQUE or MASK materials. Scene
catalogue format 4 retains the CPU fields and rejects catalogue format 3. Virtual
geometry format 3 remains unchanged because these effects do not change opaque
cluster geometry. Tile material export preserves effects and unlit identity.

Qualification covers the primary static forward path, including its optional
ray-shadow variant. Ray-hit material records do not carry view fade parameters;
reflected material fade and dynamic animation materials remain unqualified.
