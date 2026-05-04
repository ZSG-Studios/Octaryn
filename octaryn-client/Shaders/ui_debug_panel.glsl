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
