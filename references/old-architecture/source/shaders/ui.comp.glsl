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
    uint MenuRow;
    uint MenuDisplay;
    uint MenuModeWidth;
    uint MenuModeHeight;
    uint MenuFullscreen;
    uint MenuFog;
    uint MenuRenderDistance;
    uint MenuClouds;
    uint MenuSkyGradient;
    uint MenuStars;
    uint MenuSun;
    uint MenuMoon;
    uint MenuPOM;
    uint MenuPBR;
};

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

const float kEpsilon = 0.001;
const float kWidth = 1280.0;
const float kHeight = 720.0;
const float kUiScale = 2.0;
const uint kTextColumns = 30u;
const uint kMenuRows = 15u;
const uint kMenuValueEndColumn = 22u;
const uint kDebugRows = 16u;

#define GLYPH(a, b, c, d, e) ((a) | ((b) << 3u) | ((c) << 6u) | ((d) << 9u) | ((e) << 12u))

const uint kGlyphBlank = 0u;
const uint kGlyphDot = GLYPH(0u, 0u, 0u, 0u, 2u);
const uint kGlyph0 = GLYPH(7u, 5u, 5u, 5u, 7u);
const uint kGlyph1 = GLYPH(2u, 6u, 2u, 2u, 7u);
const uint kGlyph2 = GLYPH(7u, 1u, 7u, 4u, 7u);
const uint kGlyph3 = GLYPH(7u, 1u, 7u, 1u, 7u);
const uint kGlyph4 = GLYPH(5u, 5u, 7u, 1u, 1u);
const uint kGlyph5 = GLYPH(7u, 4u, 7u, 1u, 7u);
const uint kGlyph6 = GLYPH(7u, 4u, 7u, 5u, 7u);
const uint kGlyph7 = GLYPH(7u, 1u, 1u, 1u, 1u);
const uint kGlyph8 = GLYPH(7u, 5u, 7u, 5u, 7u);
const uint kGlyph9 = GLYPH(7u, 5u, 7u, 1u, 7u);
const uint kGlyphE = GLYPH(7u, 4u, 6u, 4u, 7u);
const uint kGlyphA = GLYPH(2u, 5u, 7u, 5u, 5u);
const uint kGlyphB = GLYPH(6u, 5u, 6u, 5u, 6u);
const uint kGlyphC = GLYPH(3u, 4u, 4u, 4u, 3u);
const uint kGlyphD = GLYPH(6u, 5u, 5u, 5u, 6u);
const uint kGlyphF = GLYPH(7u, 4u, 6u, 4u, 4u);
const uint kGlyphG = GLYPH(3u, 4u, 5u, 5u, 3u);
const uint kGlyphH = GLYPH(5u, 5u, 7u, 5u, 5u);
const uint kGlyphI = GLYPH(7u, 2u, 2u, 2u, 7u);
const uint kGlyphK = GLYPH(5u, 5u, 6u, 5u, 5u);
const uint kGlyphL = GLYPH(4u, 4u, 4u, 4u, 7u);
const uint kGlyphM = GLYPH(5u, 7u, 7u, 5u, 5u);
const uint kGlyphN = GLYPH(5u, 7u, 7u, 7u, 5u);
const uint kGlyphO = GLYPH(2u, 5u, 5u, 5u, 2u);
const uint kGlyphP = GLYPH(6u, 5u, 6u, 4u, 4u);
const uint kGlyphQ = GLYPH(2u, 5u, 5u, 7u, 3u);
const uint kGlyphR = GLYPH(6u, 5u, 6u, 5u, 5u);
const uint kGlyphS = GLYPH(7u, 4u, 7u, 1u, 7u);
const uint kGlyphT = GLYPH(7u, 2u, 2u, 2u, 2u);
const uint kGlyphU = GLYPH(5u, 5u, 5u, 5u, 7u);
const uint kGlyphV = GLYPH(5u, 5u, 5u, 5u, 2u);
const uint kGlyphW = GLYPH(5u, 5u, 7u, 7u, 5u);
const uint kGlyphX = GLYPH(5u, 5u, 2u, 5u, 5u);
const uint kGlyphY = GLYPH(5u, 5u, 2u, 2u, 2u);
const uint kGlyphZ = GLYPH(7u, 1u, 2u, 4u, 7u);

const uint kCharSpace = 0u;
const uint kCharDot = 46u;
const uint kChar0 = 48u;
const uint kChar1 = 49u;
const uint kChar2 = 50u;
const uint kChar3 = 51u;
const uint kChar4 = 52u;
const uint kChar5 = 53u;
const uint kChar6 = 54u;
const uint kChar7 = 55u;
const uint kChar8 = 56u;
const uint kChar9 = 57u;
const uint kCharA = 65u;
const uint kCharB = 66u;
const uint kCharC = 67u;
const uint kCharD = 68u;
const uint kCharE = 69u;
const uint kCharF = 70u;
const uint kCharG = 71u;
const uint kCharH = 72u;
const uint kCharI = 73u;
const uint kCharK = 75u;
const uint kCharL = 76u;
const uint kCharM = 77u;
const uint kCharN = 78u;
const uint kCharO = 79u;
const uint kCharP = 80u;
const uint kCharR = 82u;
const uint kCharS = 83u;
const uint kCharT = 84u;
const uint kCharU = 85u;
const uint kCharV = 86u;
const uint kCharW = 87u;
const uint kCharX = 88u;
const uint kCharY = 89u;
const uint kCharZ = 90u;

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

uint get_digit_glyph(uint digit)
{
    switch (digit)
    {
    case 0u: return kGlyph0;
    case 1u: return kGlyph1;
    case 2u: return kGlyph2;
    case 3u: return kGlyph3;
    case 4u: return kGlyph4;
    case 5u: return kGlyph5;
    case 6u: return kGlyph6;
    case 7u: return kGlyph7;
    case 8u: return kGlyph8;
    case 9u: return kGlyph9;
    default: return kGlyphBlank;
    }
}

uint get_code_glyph(uint code)
{
    switch (code)
    {
    case kCharDot: return kGlyphDot;
    case kChar0: return kGlyph0;
    case kChar1: return kGlyph1;
    case kChar2: return kGlyph2;
    case kChar3: return kGlyph3;
    case kChar4: return kGlyph4;
    case kChar5: return kGlyph5;
    case kChar6: return kGlyph6;
    case kChar7: return kGlyph7;
    case kChar8: return kGlyph8;
    case kChar9: return kGlyph9;
    case kCharA: return kGlyphA;
    case kCharB: return kGlyphB;
    case kCharC: return kGlyphC;
    case kCharD: return kGlyphD;
    case kCharE: return kGlyphE;
    case kCharF: return kGlyphF;
    case kCharG: return kGlyphG;
    case kCharH: return kGlyphH;
    case kCharI: return kGlyphI;
    case kCharK: return kGlyphK;
    case kCharL: return kGlyphL;
    case kCharM: return kGlyphM;
    case kCharN: return kGlyphN;
    case kCharO: return kGlyphO;
    case kCharP: return kGlyphP;
    case kCharR: return kGlyphR;
    case kCharS: return kGlyphS;
    case kCharT: return kGlyphT;
    case kCharU: return kGlyphU;
    case kCharV: return kGlyphV;
    case kCharW: return kGlyphW;
    case kCharX: return kGlyphX;
    case kCharY: return kGlyphY;
    case kCharZ: return kGlyphZ;
    default: return kGlyphBlank;
    }
}

bool is_glyph_pixel(uint glyph, uint x, uint y)
{
    return (glyph & (1u << ((4u - y) * 3u + (2u - x)))) != 0u;
}

uint fit_panel_font_scale(uint requested_font_scale, uint rows, int viewport_height)
{
    int available_height = max(1, viewport_height - 16);
    int needed_units = int(rows) * 6 + 8;
    uint fitted = uint(max(1, available_height / needed_units));
    return max(1u, min(requested_font_scale, fitted));
}

uint get_hundredths_value_glyph_from(uint value, uint column, uint start_column)
{
    uint hundreds = (value / 10000u) % 10u;
    uint tens = (value / 1000u) % 10u;
    uint ones = (value / 100u) % 10u;
    uint tenths = (value / 10u) % 10u;
    uint hundredths = value % 10u;
    bool show_hundreds = hundreds > 0u;
    bool show_tens = show_hundreds || tens > 0u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    switch (relative_column)
    {
    case 0u: return show_hundreds ? get_digit_glyph(hundreds) : kGlyphBlank;
    case 1u: return show_tens ? get_digit_glyph(tens) : kGlyphBlank;
    case 2u: return get_digit_glyph(ones);
    case 3u: return kGlyphDot;
    case 4u: return get_digit_glyph(tenths);
    case 5u: return get_digit_glyph(hundredths);
    default: return kGlyphBlank;
    }
}

uint get_hundredths_value_glyph(uint value, uint column)
{
    return get_hundredths_value_glyph_from(value, column, 5u);
}

uint get_menu_bool_value_glyph(bool enabled, uint column)
{
    if (enabled)
    {
        return get_code_glyph(column == kMenuValueEndColumn - 1u ? kCharO :
                              column == kMenuValueEndColumn ? kCharN :
                              kCharSpace);
    }
    return get_code_glyph(column == kMenuValueEndColumn - 2u ? kCharO :
                          column == kMenuValueEndColumn - 1u ? kCharF :
                          column == kMenuValueEndColumn ? kCharF :
                          kCharSpace);
}

uint get_menu_display_value_glyph(uint value, uint column)
{
    uint tens = (value / 10u) % 10u;
    uint ones = value % 10u;
    if (column == kMenuValueEndColumn - 1u)
    {
        return tens > 0u ? get_digit_glyph(tens) : kGlyphBlank;
    }
    if (column == kMenuValueEndColumn)
    {
        return get_digit_glyph(ones);
    }
    return kGlyphBlank;
}

uint get_menu_integer_value_glyph(uint value, uint column)
{
    uint hundreds = (value / 100u) % 10u;
    uint tens = (value / 10u) % 10u;
    uint ones = value % 10u;
    bool show_hundreds = hundreds > 0u;
    bool show_tens = show_hundreds || tens > 0u;
    if (column == kMenuValueEndColumn - 2u)
    {
        return show_hundreds ? get_digit_glyph(hundreds) : kGlyphBlank;
    }
    if (column == kMenuValueEndColumn - 1u)
    {
        return show_tens ? get_digit_glyph(tens) : kGlyphBlank;
    }
    if (column == kMenuValueEndColumn)
    {
        return get_digit_glyph(ones);
    }
    return kGlyphBlank;
}

uint get_menu_resolution_value_glyph(uint width, uint height, uint column)
{
    const uint start_column = kMenuValueEndColumn - 8u;
    uint w_thousands = (width / 1000u) % 10u;
    uint w_hundreds = (width / 100u) % 10u;
    uint w_tens = (width / 10u) % 10u;
    uint w_ones = width % 10u;
    uint h_thousands = (height / 1000u) % 10u;
    uint h_hundreds = (height / 100u) % 10u;
    uint h_tens = (height / 10u) % 10u;
    uint h_ones = height % 10u;
    bool show_w_thousands = w_thousands > 0u;
    bool show_w_hundreds = show_w_thousands || w_hundreds > 0u;
    bool show_w_tens = show_w_hundreds || w_tens > 0u;
    bool show_h_thousands = h_thousands > 0u;
    bool show_h_hundreds = show_h_thousands || h_hundreds > 0u;
    bool show_h_tens = show_h_hundreds || h_tens > 0u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    switch (relative_column)
    {
    case 0u: return show_w_thousands ? get_digit_glyph(w_thousands) : kGlyphBlank;
    case 1u: return show_w_hundreds ? get_digit_glyph(w_hundreds) : kGlyphBlank;
    case 2u: return show_w_tens ? get_digit_glyph(w_tens) : kGlyphBlank;
    case 3u: return get_digit_glyph(w_ones);
    case 4u: return kGlyphX;
    case 5u: return show_h_thousands ? get_digit_glyph(h_thousands) : kGlyphBlank;
    case 6u: return show_h_hundreds ? get_digit_glyph(h_hundreds) : kGlyphBlank;
    case 7u: return show_h_tens ? get_digit_glyph(h_tens) : kGlyphBlank;
    case 8u: return get_digit_glyph(h_ones);
    default: return kGlyphBlank;
    }
}

uint get_tenths_value_glyph_from(uint value, uint column, uint start_column)
{
    uint thousands = (value / 10000u) % 10u;
    uint hundreds = (value / 1000u) % 10u;
    uint tens = (value / 100u) % 10u;
    uint ones = (value / 10u) % 10u;
    uint tenths = value % 10u;
    bool show_thousands = thousands > 0u;
    bool show_hundreds = show_thousands || hundreds > 0u;
    bool show_tens = show_hundreds || tens > 0u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    switch (relative_column)
    {
    case 0u: return show_thousands ? get_digit_glyph(thousands) : kGlyphBlank;
    case 1u: return show_hundreds ? get_digit_glyph(hundreds) : kGlyphBlank;
    case 2u: return show_tens ? get_digit_glyph(tens) : kGlyphBlank;
    case 3u: return get_digit_glyph(ones);
    case 4u: return kGlyphDot;
    case 5u: return get_digit_glyph(tenths);
    default: return kGlyphBlank;
    }
}

uint get_tenths_value_glyph(uint value, uint column)
{
    return get_tenths_value_glyph_from(value, column, 5u);
}

uint get_uint_value_glyph_from(uint value, uint column, uint start_column)
{
    uint hundred_thousands = (value / 100000u) % 10u;
    uint ten_thousands = (value / 10000u) % 10u;
    uint thousands = (value / 1000u) % 10u;
    uint hundreds = (value / 100u) % 10u;
    uint tens = (value / 10u) % 10u;
    uint ones = value % 10u;
    bool show_hundred_thousands = hundred_thousands > 0u;
    bool show_ten_thousands = show_hundred_thousands || ten_thousands > 0u;
    bool show_thousands = show_ten_thousands || thousands > 0u;
    bool show_hundreds = show_thousands || hundreds > 0u;
    bool show_tens = show_hundreds || tens > 0u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    switch (relative_column)
    {
    case 0u: return show_hundred_thousands ? get_digit_glyph(hundred_thousands) : kGlyphBlank;
    case 1u: return show_ten_thousands ? get_digit_glyph(ten_thousands) : kGlyphBlank;
    case 2u: return show_thousands ? get_digit_glyph(thousands) : kGlyphBlank;
    case 3u: return show_hundreds ? get_digit_glyph(hundreds) : kGlyphBlank;
    case 4u: return show_tens ? get_digit_glyph(tens) : kGlyphBlank;
    case 5u: return get_digit_glyph(ones);
    default: return kGlyphBlank;
    }
}

uint get_debug_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        if (column < 6u) { return get_code_glyph(uint[](kCharM,kCharE,kCharT,kCharR,kCharI,kCharC)[column]); }
        if (column >= 10u && column < 12u) { return get_code_glyph(uint[](kCharM,kCharS)[column - 10u]); }
        if (column >= 21u && column < 24u) { return get_code_glyph(uint[](kCharF,kCharP,kCharS)[column - 21u]); }
        return kGlyphBlank;
    }
    if (row == 1u) { if (column < 7u) { return get_code_glyph(uint[](kCharC,kCharU,kCharR,kCharR,kCharE,kCharN,kCharT)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(FrameTimeHundredths, column, 9u); } return get_tenths_value_glyph_from(FPSTenths, column, 20u); }
    if (row == 2u) { if (column < 7u) { return get_code_glyph(uint[](kCharA,kCharV,kCharE,kCharR,kCharA,kCharG,kCharE)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(ProfileFrameTimeHundredths, column, 9u); } return get_tenths_value_glyph_from(FPSAverageTenths, column, 20u); }
    if (row == 3u) { if (column < 6u) { return get_code_glyph(uint[](kChar1,kCharSpace,kCharL,kCharO,kCharW,kCharSpace)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(MSLow1Hundredths, column, 9u); } return get_tenths_value_glyph_from(FPSLow1Tenths, column, 20u); }
    if (row == 4u) { if (column < 7u) { return get_code_glyph(uint[](kChar0,kChar1,kCharSpace,kCharL,kCharO,kCharW,kCharSpace)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(MSLow01Hundredths, column, 9u); } return get_tenths_value_glyph_from(FPSLow01Tenths, column, 20u); }
    if (row == 5u) { if (column < 6u) { return get_code_glyph(uint[](kCharL,kCharO,kCharW,kCharSpace,kCharX,kChar5)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(MSLowX5Hundredths, column, 9u); } return get_tenths_value_glyph_from(FPSLowX5Tenths, column, 20u); }
    if (row == 6u) { if (column < 7u) { return get_code_glyph(uint[](kCharL,kCharO,kCharW,kCharSpace,kCharX,kChar1,kChar0)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(MSLowX10Hundredths, column, 9u); } return get_tenths_value_glyph_from(FPSLowX10Tenths, column, 20u); }
    if (row == 7u) { if (column < 5u) { return get_code_glyph(uint[](kCharW,kCharO,kCharR,kCharS,kCharT)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(MSWorstHundredths, column, 9u); } return get_tenths_value_glyph_from(FPSWorstTenths, column, 20u); }
    if (row == 8u) { if (column < 6u) { return get_code_glyph(uint[](kCharW,kCharA,kCharR,kCharM,kCharU,kCharP)[column]); } if (column < 18u) { return get_hundredths_value_glyph_from(WarmupElapsedHundredths, column, 9u); } return get_hundredths_value_glyph_from(WarmupTotalHundredths, column, 20u); }
    if (row == 9u) { if (column < 7u) { return get_code_glyph(uint[](kCharS,kCharA,kCharM,kCharP,kCharL,kCharE,kCharS)[column]); } return get_uint_value_glyph_from(SampleCount, column, 20u); }
    if (row == 10u) { return column < 8u ? get_code_glyph(uint[](kCharC,kCharP,kCharU,kCharSpace,kCharL,kCharO,kCharA,kCharD)[column]) : get_hundredths_value_glyph_from(CpuLoadHundredths, column, 20u); }
    if (row == 11u) { return column < 8u ? get_code_glyph(uint[](kCharG,kCharP,kCharU,kCharSpace,kCharL,kCharO,kCharA,kCharD)[column]) : get_hundredths_value_glyph_from(GpuLoadHundredths, column, 20u); }
    if (row == 12u) { return column < 12u ? get_code_glyph(uint[](kCharR,kCharA,kCharM,kCharSpace,kCharU,kCharS,kCharE,kCharD,kCharSpace,kCharG,kCharI,kCharB)[column]) : get_hundredths_value_glyph_from(CpuRamHundredthsGiB, column, 20u); }
    if (row == 13u) { return column < 13u ? get_code_glyph(uint[](kCharV,kCharR,kCharA,kCharM,kCharSpace,kCharU,kCharS,kCharE,kCharD,kCharSpace,kCharG,kCharI,kCharB)[column]) : get_hundredths_value_glyph_from(GpuVramHundredthsGiB, column, 20u); }
    if (row == 14u) { return column < 8u ? get_code_glyph(uint[](kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharM,kCharS)[column]) : get_hundredths_value_glyph_from(WorldTimeHundredths, column, 20u); }
    if (row == 15u) { return column < 9u ? get_code_glyph(uint[](kCharR,kCharE,kCharN,kCharD,kCharE,kCharR,kCharSpace,kCharM,kCharS)[column]) : get_hundredths_value_glyph_from(RenderTimeHundredths, column, 20u); }

    return kGlyphBlank;
}

bool is_debug_text_pixel(ivec2 screen, ivec2 origin, uint font_scale)
{
    int local_x = screen.x - origin.x;
    int local_y = screen.y - origin.y;
    if (local_x < 0 || local_y < 0)
    {
        return false;
    }

    uint glyph_advance = 4u * font_scale;
    uint line_advance = 6u * font_scale;
    uint column = uint(local_x) / glyph_advance;
    uint row = uint(local_y) / line_advance;
    if (column >= kTextColumns || row >= kDebugRows)
    {
        return false;
    }

    uint glyph_x = (uint(local_x) / font_scale) % 4u;
    uint glyph_y = (uint(local_y) / font_scale) % 6u;
    if (glyph_x >= 3u || glyph_y >= 5u)
    {
        return false;
    }

    return is_glyph_pixel(get_debug_glyph(row, column), glyph_x, glyph_y);
}

uint get_menu_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        uint label = column < 7u ? uint[](kCharD,kCharI,kCharS,kCharP,kCharL,kCharA,kCharY)[column] : kCharSpace;
        if (column < 7u) { return get_code_glyph(label); }
        return get_menu_display_value_glyph(MenuDisplay, column);
    }
    else if (row == 1u)
    {
        if (column < 10u)
        {
            return get_code_glyph(uint[](kCharR,kCharE,kCharS,kCharO,kCharL,kCharU,kCharT,kCharI,kCharO,kCharN)[column]);
        }
        return get_menu_resolution_value_glyph(MenuModeWidth, MenuModeHeight, column);
    }
    else if (row == 2u)
    {
        if (column < 10u) { return get_code_glyph(uint[](kCharF,kCharU,kCharL,kCharL,kCharS,kCharC,kCharR,kCharE,kCharE,kCharN)[column]); }
        return get_menu_bool_value_glyph(MenuFullscreen != 0u, column);
    }
    else if (row == 3u)
    {
        if (column < 11u) { return get_code_glyph(uint[](kCharR,kCharE,kCharN,kCharD,kCharE,kCharR,kCharSpace,kCharD,kCharI,kCharS,kCharT)[column]); }
        return get_menu_integer_value_glyph(MenuRenderDistance, column);
    }
    else if (row == 4u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharF,kCharO,kCharG)[column]); }
        return get_menu_bool_value_glyph(MenuFog != 0u, column);
    }
    else if (row == 5u)
    {
        if (column < 6u) { return get_code_glyph(uint[](kCharC,kCharL,kCharO,kCharU,kCharD,kCharS)[column]); }
        return get_menu_bool_value_glyph(MenuClouds != 0u, column);
    }
    else if (row == 6u)
    {
        if (column < 9u) { return get_code_glyph(uint[](kCharS,kCharK,kCharY,kCharSpace,kCharC,kCharO,kCharL,kCharO,kCharR)[column]); }
        return get_menu_bool_value_glyph(MenuSkyGradient != 0u, column);
    }
    else if (row == 7u)
    {
        if (column < 5u) { return get_code_glyph(uint[](kCharS,kCharT,kCharA,kCharR,kCharS)[column]); }
        return get_menu_bool_value_glyph(MenuStars != 0u, column);
    }
    else if (row == 8u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharS,kCharU,kCharN)[column]); }
        return get_menu_bool_value_glyph(MenuSun != 0u, column);
    }
    else if (row == 9u)
    {
        if (column < 4u) { return get_code_glyph(uint[](kCharM,kCharO,kCharO,kCharN)[column]); }
        return get_menu_bool_value_glyph(MenuMoon != 0u, column);
    }
    else if (row == 10u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharP,kCharO,kCharM)[column]); }
        return get_menu_bool_value_glyph(MenuPOM != 0u, column);
    }
    else if (row == 11u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharP,kCharB,kCharR)[column]); }
        return get_menu_bool_value_glyph(MenuPBR != 0u, column);
    }
    else if (row == 12u)
    {
        return column < 5u ? get_code_glyph(uint[](kCharA,kCharP,kCharP,kCharL,kCharY)[column]) : kGlyphBlank;
    }
    else if (row == 13u)
    {
        return column < 5u ? get_code_glyph(uint[](kCharC,kCharL,kCharO,kCharS,kCharE)[column]) : kGlyphBlank;
    }
    else if (row == 14u)
    {
        return column < 4u ? get_code_glyph(uint[](kCharE,kCharX,kCharI,kCharT)[column]) : kGlyphBlank;
    }

    return kGlyphBlank;
}

bool is_menu_text_pixel(ivec2 screen, ivec2 origin, uint font_scale)
{
    int local_x = screen.x - origin.x;
    int local_y = screen.y - origin.y;
    if (local_x < 0 || local_y < 0)
    {
        return false;
    }

    uint glyph_advance = 4u * font_scale;
    uint line_advance = 6u * font_scale;
    uint column = uint(local_x) / glyph_advance;
    uint row = uint(local_y) / line_advance;
    if (column >= kTextColumns || row >= kMenuRows)
    {
        return false;
    }

    row = kMenuRows - 1u - row;
    uint glyph_x = (uint(local_x) / font_scale) % 4u;
    uint glyph_y = (uint(local_y) / font_scale) % 6u;
    if (glyph_x >= 3u || glyph_y >= 5u)
    {
        return false;
    }

    return is_glyph_pixel(get_menu_glyph(row, column), glyph_x, glyph_y);
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
