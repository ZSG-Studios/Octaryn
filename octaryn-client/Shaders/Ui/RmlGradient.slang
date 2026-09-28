// Gradient shader decorator: linear / radial / conic gradients with up to 16
// color stops, optional repeat. Element geometry keeps the packed Rml::Vertex
// layout; texcoords carry element-space pixel coordinates.
ByteAddressBuffer vertices;
uniform float4 transformRow0;
uniform float4 transformRow1;
uniform float4 transformRow2;
uniform float4 transformRow3;
uniform float4 translationScale; // translate.xy, 2/dims.xy
uniform float4 gradient_p;       // linear: p0, radial/conic: center (x, y)
uniform float4 gradient_v;       // linear: p1-p0, radial: 1/radius, conic: angle vector
uniform float4 gradient_misc;    // func, num_stops, unused, unused
uniform float4 stop_colors[16];
uniform float4 stop_positions[4]; // 16 positions packed 4 per row

struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };

[shader("vertex")]
Varying vertex_main(uint id : SV_VertexID) {
    uint offset = id * 20;
    float2 position = asfloat(vertices.Load2(offset)) + translationScale.xy;
    uint color = vertices.Load(offset + 8);
    float4 local = float4(position, 0, 1);
    float4 transformed = float4(dot(transformRow0, local), dot(transformRow1, local),
                                dot(transformRow2, local), dot(transformRow3, local));
    Varying result;
    result.position = float4(transformed.x * translationScale.z - transformed.w,
                             transformed.w - transformed.y * translationScale.w, 0, transformed.w);
    result.uv = asfloat(vertices.Load2(offset + 12));
    result.color = float4(color & 255, (color >> 8) & 255,
                         (color >> 16) & 255, color >> 24) / 255.0;
    return result;
}

static const float PI = 3.14159265;

float stop_position(int index) { return stop_positions[index / 4][index % 4]; }

float4 mix_stop_colors(float t, int num_stops) {
    float4 color = stop_colors[0];
    for (int i = 1; i < num_stops; ++i) {
        color = lerp(color, stop_colors[i],
                     smoothstep(stop_position(i - 1), stop_position(i), t));
    }
    return color;
}

[shader("fragment")]
float4 fragment_main(Varying v) : SV_Target {
    int func = int(gradient_misc.x + 0.5);
    int num_stops = clamp(int(gradient_misc.y + 0.5), 1, 16);
    float2 p = gradient_p.xy;
    float2 dir = gradient_v.xy;
    float t = 0.0;
    if (func == 0 || func == 3) {          // linear / repeating linear
        float2 diff = v.uv - p;
        t = dot(dir, diff) / max(dot(dir, dir), 1e-9);
    } else if (func == 1 || func == 4) {   // radial / repeating radial
        float2 diff = v.uv - p;
        t = length(dir * diff);
    } else {                               // conic / repeating conic
        float2x2 rotation = float2x2(dir.x, -dir.y, dir.y, dir.x);
        float2 diff = mul(rotation, v.uv - p);
        t = 0.5 + atan2(-diff.x, diff.y) / (2.0 * PI);
    }
    if (func >= 3) {
        float t0 = stop_position(0);
        float t1 = stop_position(num_stops - 1);
        t = t0 + fmod(t - t0, max(t1 - t0, 1e-6));
    }
    return v.color * mix_stop_colors(t, num_stops);
}
