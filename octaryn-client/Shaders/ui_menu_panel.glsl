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

uint get_menu_present_mode_value_glyph(uint mode, uint column)
{
    const uint start_column = kMenuValueEndColumn - 8u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    if (mode == 0u)
    {
        if (relative_column < 9u)
        {
            return get_code_glyph(uint[](kCharI,kCharM,kCharM,kCharE,kCharD,kCharI,kCharA,kCharT,kCharE)[relative_column]);
        }
    }
    else if (mode == 1u)
    {
        if (relative_column < 7u)
        {
            return get_code_glyph(uint[](kCharM,kCharA,kCharI,kCharL,kCharB,kCharO,kCharX)[relative_column]);
        }
    }
    else
    {
        if (relative_column < 5u)
        {
            return get_code_glyph(uint[](kCharV,kCharS,kCharY,kCharN,kCharC)[relative_column]);
        }
    }
    return kGlyphBlank;
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

uint get_menu_port_value_glyph(uint value, uint column)
{
    const uint start_column = kMenuValueEndColumn - 4u;
    uint ten_thousands = (value / 10000u) % 10u;
    uint thousands = (value / 1000u) % 10u;
    uint hundreds = (value / 100u) % 10u;
    uint tens = (value / 10u) % 10u;
    uint ones = value % 10u;
    bool show_ten_thousands = ten_thousands > 0u;
    bool show_thousands = show_ten_thousands || thousands > 0u;
    bool show_hundreds = show_thousands || hundreds > 0u;
    bool show_tens = show_hundreds || tens > 0u;
    uint relative_column = column >= start_column ? column - start_column : 99u;
    switch (relative_column)
    {
    case 0u: return show_ten_thousands ? get_digit_glyph(ten_thousands) : kGlyphBlank;
    case 1u: return show_thousands ? get_digit_glyph(thousands) : kGlyphBlank;
    case 2u: return show_hundreds ? get_digit_glyph(hundreds) : kGlyphBlank;
    case 3u: return show_tens ? get_digit_glyph(tens) : kGlyphBlank;
    case 4u: return get_digit_glyph(ones);
    default: return kGlyphBlank;
    }
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

uint get_menu_word_glyph(uint column, uint count, uint chars[16])
{
    return column < count ? get_code_glyph(chars[column]) : kGlyphBlank;
}

uint get_menu_packed_text_glyph(uint column, uint start_column)
{
    if (column < start_column || column >= start_column + 16u)
    {
        return kGlyphBlank;
    }
    uint relative_column = column - start_column;
    uint word = relative_column < 4u ? MenuServerAddress0 :
                relative_column < 8u ? MenuServerAddress1 :
                relative_column < 12u ? MenuServerAddress2 :
                MenuServerAddress3;
    uint code = (word >> ((relative_column % 4u) * 8u)) & 255u;
    return code == 0u ? kGlyphBlank : get_code_glyph(code);
}

uint get_menu_packed_world_name_glyph(uint column, uint start_column)
{
    if (column < start_column || column >= start_column + 16u)
    {
        return kGlyphBlank;
    }
    uint relative_column = column - start_column;
    uint word = relative_column < 4u ? MenuWorldName0 :
                relative_column < 8u ? MenuWorldName1 :
                relative_column < 12u ? MenuWorldName2 :
                MenuWorldName3;
    uint code = (word >> ((relative_column % 4u) * 8u)) & 255u;
    return code == 0u ? kGlyphBlank : get_code_glyph(code);
}

uint get_ingame_menu_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        return get_menu_word_glyph(column, 9u, uint[16](kCharP,kCharA,kCharU,kCharS,kCharE,kCharSpace,kCharM,kCharE,kCharN,kCharU,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 2u)
    {
        return get_menu_word_glyph(column, 6u, uint[16](kCharR,kCharE,kCharS,kCharU,kCharM,kCharE,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 3u)
    {
        return get_menu_word_glyph(column, 10u, uint[16](kCharS,kCharA,kCharV,kCharE,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 4u)
    {
        return get_menu_word_glyph(column, 8u, uint[16](kCharS,kCharE,kCharT,kCharT,kCharI,kCharN,kCharG,kCharS,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 5u)
    {
        return get_menu_word_glyph(column, 13u, uint[16](kCharL,kCharE,kCharA,kCharV,kCharE,kCharSpace,kCharS,kCharE,kCharS,kCharS,kCharI,kCharO,kCharN,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 15u)
    {
        return get_menu_word_glyph(column, 4u, uint[16](kCharE,kCharX,kCharI,kCharT,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    return kGlyphBlank;
}

uint get_main_menu_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        return get_menu_word_glyph(column, 9u, uint[16](kCharM,kCharA,kCharI,kCharN,kCharSpace,kCharM,kCharE,kCharN,kCharU,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 2u)
    {
        return get_menu_word_glyph(column, 12u, uint[16](kCharS,kCharI,kCharN,kCharG,kCharL,kCharE,kCharP,kCharL,kCharA,kCharY,kCharE,kCharR,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 3u)
    {
        return get_menu_word_glyph(column, 11u, uint[16](kCharM,kCharU,kCharL,kCharT,kCharI,kCharP,kCharL,kCharA,kCharY,kCharE,kCharR,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 4u)
    {
        return get_menu_word_glyph(column, 8u, uint[16](kCharS,kCharE,kCharT,kCharT,kCharI,kCharN,kCharG,kCharS,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 15u)
    {
        return get_menu_word_glyph(column, 4u, uint[16](kCharE,kCharX,kCharI,kCharT,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    return kGlyphBlank;
}

uint get_singleplayer_menu_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        return get_menu_word_glyph(column, 12u, uint[16](kCharS,kCharI,kCharN,kCharG,kCharL,kCharE,kCharP,kCharL,kCharA,kCharY,kCharE,kCharR,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 2u)
    {
        return get_menu_word_glyph(column, 7u, uint[16](kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kChar1,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 3u)
    {
        return get_menu_word_glyph(column, 7u, uint[16](kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kChar2,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 4u)
    {
        return get_menu_word_glyph(column, 7u, uint[16](kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kChar3,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 5u)
    {
        if (column < 4u) { return get_code_glyph(uint[](kCharN,kCharA,kCharM,kCharE)[column]); }
        return get_menu_packed_world_name_glyph(column, 7u);
    }
    if (row == 6u)
    {
        return get_menu_word_glyph(column, 10u, uint[16](kCharL,kCharO,kCharA,kCharD,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 7u)
    {
        return get_menu_word_glyph(column, 12u, uint[16](kCharC,kCharR,kCharE,kCharA,kCharT,kCharE,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 8u)
    {
        return get_menu_word_glyph(column, 12u, uint[16](kCharD,kCharE,kCharL,kCharE,kCharT,kCharE,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 9u)
    {
        return get_menu_word_glyph(column, 10u, uint[16](kCharS,kCharA,kCharV,kCharE,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 14u)
    {
        return get_menu_word_glyph(column, 4u, uint[16](kCharB,kCharA,kCharC,kCharK,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    return kGlyphBlank;
}

uint get_multiplayer_menu_glyph(uint row, uint column)
{
    if (row == 0u)
    {
        return get_menu_word_glyph(column, 11u, uint[16](kCharM,kCharU,kCharL,kCharT,kCharI,kCharP,kCharL,kCharA,kCharY,kCharE,kCharR,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 2u)
    {
        if (column < 7u) { return get_code_glyph(uint[](kCharA,kCharD,kCharD,kCharR,kCharE,kCharS)[column]); }
        return get_menu_packed_text_glyph(column, 10u);
    }
    if (row == 3u)
    {
        if (column < 4u) { return get_code_glyph(uint[](kCharP,kCharO,kCharR,kCharT)[column]); }
        return get_menu_port_value_glyph(MenuServerPort, column);
    }
    if (row == 4u)
    {
        return get_menu_word_glyph(column, 14u, uint[16](kCharC,kCharO,kCharN,kCharN,kCharE,kCharC,kCharT,kCharSpace,kCharS,kCharE,kCharR,kCharV,kCharE,kCharR,kCharSpace,kCharSpace));
    }
    if (row == 5u)
    {
        return get_menu_word_glyph(column, 13u, uint[16](kCharC,kCharO,kCharN,kCharN,kCharE,kCharC,kCharT,kCharSpace,kCharL,kCharO,kCharC,kCharA,kCharL,kCharSpace,kCharSpace,kCharSpace));
    }
    if (row == 14u)
    {
        return get_menu_word_glyph(column, 4u, uint[16](kCharB,kCharA,kCharC,kCharK,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace,kCharSpace));
    }
    return kGlyphBlank;
}

uint get_settings_menu_glyph(uint row, uint column)
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
        if (column < 5u) { return get_code_glyph(uint[](kCharV,kCharS,kCharY,kCharN,kCharC)[column]); }
        return get_menu_present_mode_value_glyph(MenuPresentMode, column);
    }
    else if (row == 4u)
    {
        if (column < 11u) { return get_code_glyph(uint[](kCharR,kCharE,kCharN,kCharD,kCharE,kCharR,kCharSpace,kCharD,kCharI,kCharS,kCharT)[column]); }
        return get_menu_integer_value_glyph(MenuRenderDistance, column);
    }
    else if (row == 5u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharF,kCharO,kCharG)[column]); }
        return get_menu_bool_value_glyph(MenuFog != 0u, column);
    }
    else if (row == 6u)
    {
        if (column < 6u) { return get_code_glyph(uint[](kCharC,kCharL,kCharO,kCharU,kCharD,kCharS)[column]); }
        return get_menu_bool_value_glyph(MenuClouds != 0u, column);
    }
    else if (row == 7u)
    {
        if (column < 9u) { return get_code_glyph(uint[](kCharS,kCharK,kCharY,kCharSpace,kCharC,kCharO,kCharL,kCharO,kCharR)[column]); }
        return get_menu_bool_value_glyph(MenuSkyGradient != 0u, column);
    }
    else if (row == 8u)
    {
        if (column < 5u) { return get_code_glyph(uint[](kCharS,kCharT,kCharA,kCharR,kCharS)[column]); }
        return get_menu_bool_value_glyph(MenuStars != 0u, column);
    }
    else if (row == 9u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharS,kCharU,kCharN)[column]); }
        return get_menu_bool_value_glyph(MenuSun != 0u, column);
    }
    else if (row == 10u)
    {
        if (column < 4u) { return get_code_glyph(uint[](kCharM,kCharO,kCharO,kCharN)[column]); }
        return get_menu_bool_value_glyph(MenuMoon != 0u, column);
    }
    else if (row == 11u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharP,kCharO,kCharM)[column]); }
        return get_menu_bool_value_glyph(MenuPOM != 0u, column);
    }
    else if (row == 12u)
    {
        if (column < 3u) { return get_code_glyph(uint[](kCharP,kCharB,kCharR)[column]); }
        return get_menu_bool_value_glyph(MenuPBR != 0u, column);
    }
    else if (row == 13u)
    {
        return column < 5u ? get_code_glyph(uint[](kCharA,kCharP,kCharP,kCharL,kCharY)[column]) : kGlyphBlank;
    }
    else if (row == 14u)
    {
        return column < 5u ? get_code_glyph(uint[](kCharC,kCharL,kCharO,kCharS,kCharE)[column]) : kGlyphBlank;
    }
    else if (row == 15u)
    {
        return column < 4u ? get_code_glyph(uint[](kCharE,kCharX,kCharI,kCharT)[column]) : kGlyphBlank;
    }

    return kGlyphBlank;
}

uint get_menu_glyph(uint row, uint column)
{
    if (MenuScreen == 0u)
    {
        return get_main_menu_glyph(row, column);
    }
    if (MenuScreen == 1u)
    {
        return get_singleplayer_menu_glyph(row, column);
    }
    if (MenuScreen == 2u)
    {
        return get_multiplayer_menu_glyph(row, column);
    }
    if (MenuScreen == 4u)
    {
        return get_ingame_menu_glyph(row, column);
    }
    return get_settings_menu_glyph(row, column);
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
