#version 450

layout(set = 0, binding = 0) uniform sampler2DArray atlasTexture;
layout(set = 1, binding = 0, rgba16f) uniform image2D colorTexture;

layout(set = 2, binding = 0) uniform ViewportUniforms {
    ivec2 Viewport;
    ivec2 DispatchOffset;
};

layout(set = 2, binding = 1) uniform UiUniforms {
    uint Index;
    uint DebugEnabled;
    uint FPSTenths;
    uint FrameTimeHundredths;
    uint ProfileFrameTimeHundredths;
    uint FPSAverageTenths;
    uint FPSLow1Tenths;
    uint FPSLow01Tenths;
    uint FPSLowX5Tenths;
    uint FPSLowX10Tenths;
    uint FPSWorstTenths;
    uint WarmupComplete;
    uint SampleCount;
    uint MSLow1Hundredths;
    uint MSLow01Hundredths;
    uint MSLowX5Hundredths;
    uint MSLowX10Hundredths;
    uint MSWorstHundredths;
    uint WarmupElapsedHundredths;
    uint WarmupTotalHundredths;
    uint SimTimeHundredths;
    uint MiscTimeHundredths;
    uint WorldTimeHundredths;
    uint RenderTimeHundredths;
    uint RenderSetupHundredths;
    uint RenderOtherTimeHundredths;
    uint GBufferTimeHundredths;
    uint GBufferSkyHundredths;
    uint GBufferOpaqueHundredths;
    uint GBufferSpriteHundredths;
    uint PostTimeHundredths;
    uint CompositeTimeHundredths;
    uint DepthTimeHundredths;
    uint ForwardTimeHundredths;
    uint UiTimeHundredths;
    uint ImGuiTimeHundredths;
    uint SwapchainBlitHundredths;
    uint RenderSubmitHundredths;
    uint UntrackedTimeHundredths;
    uint CpuRamHundredthsGiB;
    uint GpuVramHundredthsGiB;
    uint CpuLoadHundredths;
    uint GpuLoadHundredths;
    uint MenuEnabled;
    uint MenuScreen;
    uint MenuRow;
    uint MenuAction;
    uint MenuWorldSlot;
    uint MenuStatusCode;
    uint MenuWorldExistsMask;
    uint MenuDisplay;
    uint MenuModeWidth;
    uint MenuModeHeight;
    uint MenuFullscreen;
    uint MenuPresentMode;
    uint MenuFog;
    uint MenuRenderDistance;
    uint MenuClouds;
    uint MenuSkyGradient;
    uint MenuStars;
    uint MenuSun;
    uint MenuMoon;
    uint MenuPOM;
    uint MenuPBR;
    uint MenuServerAddress0;
    uint MenuServerAddress1;
    uint MenuServerAddress2;
    uint MenuServerAddress3;
    uint MenuServerPort;
    uint MenuEditingField;
    uint MenuWorldName0;
    uint MenuWorldName1;
    uint MenuWorldName2;
    uint MenuWorldName3;
};

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

const float kEpsilon = 0.001;
const float kWidth = 1280.0;
const float kHeight = 720.0;
const float kUiScale = 2.0;
const uint kTextColumns = 30u;
const uint kMenuRows = 16u;
const uint kMenuValueEndColumn = 22u;
const uint kDebugRows = 16u;

#include "ui_text.glsl"
#include "ui_debug_panel.glsl"
#include "ui_menu_panel.glsl"

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 tone_map(vec3 color)
{
    color = max(color, vec3(0.0));
    return color / (color + vec3(1.0));
}

vec3 inverse_tone_map(vec3 color)
{
    color = clamp(color, vec3(0.0), vec3(0.999));
    return color / (vec3(1.0) - color);
}

vec4 blend_over(vec4 dst, vec4 src)
{
    float src_alpha = saturate(src.a);
    float dst_alpha = dst.a;
    vec3 color = src.rgb * src_alpha + dst.rgb * (1.0 - src_alpha);
    float alpha = src_alpha + dst_alpha * (1.0 - src_alpha);
    return vec4(color, alpha);
}

void blend_store(ivec2 coord, vec4 src)
{
    vec4 dst = imageLoad(colorTexture, coord);
    vec4 display_dst = vec4(tone_map(dst.rgb), dst.a);
    vec4 blended = blend_over(display_dst, src);
    imageStore(colorTexture, coord, vec4(inverse_tone_map(blended.rgb), blended.a));
}

void main()
{
    uvec3 thread_id = gl_GlobalInvocationID;
    ivec2 size = imageSize(colorTexture);
    ivec2 coord = ivec2(thread_id.xy) + DispatchOffset;
    if (coord.x >= size.x || coord.y >= size.y)
    {
        return;
    }

    float base_scale = max(float(Viewport.x) / kWidth, float(Viewport.y) / kHeight);
    float scale = base_scale * kUiScale;
    float block_width = 50.0 * scale;
    vec2 block_start = vec2(10.0 * scale, 10.0 * scale);
    vec2 block_end = block_start + vec2(block_width);
    vec2 pixel = vec2(float(coord.x), float(Viewport.y) - float(coord.y));
    ivec2 ui = ivec2(coord.x, Viewport.y - 1 - coord.y);

    if (MenuEnabled == 0u &&
        pixel.x > block_start.x && pixel.x < block_end.x &&
        pixel.y > block_start.y && pixel.y < block_end.y)
    {
        float x = (pixel.x - block_start.x) / block_width;
        float y = (pixel.y - block_start.y) / block_width;
        vec4 src = textureLod(atlasTexture, vec3(x, 1.0 - y, float(Index)), 0.0);
        if (src.a > kEpsilon)
        {
            blend_store(coord, src);
            return;
        }
    }

    float cross_width = 8.0 * base_scale;
    float cross_thickness = 2.0 * base_scale;
    vec2 cross_center = vec2(Viewport) * 0.5;
    vec2 cross_start1 = cross_center - vec2(cross_width, cross_thickness);
    vec2 cross_end1 = cross_center + vec2(cross_width, cross_thickness);
    vec2 cross_start2 = cross_center - vec2(cross_thickness, cross_width);
    vec2 cross_end2 = cross_center + vec2(cross_thickness, cross_width);
    if (MenuEnabled == 0u &&
        ((pixel.x > cross_start1.x && pixel.y > cross_start1.y && pixel.x < cross_end1.x && pixel.y < cross_end1.y) ||
         (pixel.x > cross_start2.x && pixel.y > cross_start2.y && pixel.x < cross_end2.x && pixel.y < cross_end2.y)))
    {
        blend_store(coord, vec4(1.0));
        return;
    }

    if (MenuEnabled != 0u)
    {
        uint font_scale = fit_panel_font_scale(max(1u, uint(scale + 0.5)), kMenuRows, Viewport.y);
        int padding = 4 * int(font_scale);
        int content_width = int(kTextColumns) * 4 * int(font_scale) - int(font_scale);
        int content_height = int(kMenuRows) * 6 * int(font_scale) - int(font_scale);
        int panel_width = content_width + padding * 2;
        int panel_height = content_height + padding * 2;
        ivec2 panel_min = ivec2((Viewport.x - panel_width) / 2, (Viewport.y - panel_height) / 2);
        ivec2 panel_max = panel_min + ivec2(panel_width, panel_height);
        blend_store(coord, vec4(0.0, 0.0, 0.0, 0.35));
        if (ui.x >= panel_min.x && ui.x < panel_max.x && ui.y >= panel_min.y && ui.y < panel_max.y)
        {
            blend_store(coord, vec4(0.08, 0.08, 0.10, 0.92));
            ivec2 text_origin = panel_min + ivec2(padding, padding);
            int line_advance = 6 * int(font_scale);
            int row_top = text_origin.y + int(kMenuRows - 1u - MenuRow) * line_advance - int(font_scale);
            int row_bottom = row_top + line_advance;
            if (ui.y >= row_top && ui.y < row_bottom)
            {
                blend_store(coord, vec4(0.16, 0.25, 0.34, 0.85));
            }
            if (is_menu_text_pixel(ui, text_origin, font_scale))
            {
                blend_store(coord, vec4(1.0, 1.0, 1.0, 0.98));
            }
        }
    }

    if (DebugEnabled == 0u)
    {
        return;
    }

    uint font_scale = fit_panel_font_scale(max(1u, uint(scale + 0.5)), kDebugRows, Viewport.y);
    int padding = 4 * int(font_scale);
    int margin = 6 * int(font_scale);
    int content_width = int(kTextColumns) * 4 * int(font_scale) - int(font_scale);
    int content_height = int(kDebugRows) * 6 * int(font_scale) - int(font_scale);
    int panel_height = content_height + padding * 2;
    ivec2 panel_min = ivec2(margin, max(margin, Viewport.y - margin - panel_height));
    ivec2 panel_max = panel_min + ivec2(content_width + padding * 2, panel_height);
    if (DebugEnabled != 0u && ui.x >= panel_min.x && ui.x < panel_max.x && ui.y >= panel_min.y && ui.y < panel_max.y)
    {
        blend_store(coord, vec4(0.0, 0.0, 0.0, 0.55));
        if (is_debug_text_pixel(ui, panel_min + ivec2(padding, padding), font_scale))
        {
            blend_store(coord, vec4(1.0, 1.0, 1.0, 0.95));
        }
    }
}
