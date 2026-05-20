uint get_selected_world_glyph(uint column)
{
    if ((MenuWorldExistsMask & (1u << MenuWorldSlot)) == 0u)
    {
        return get_menu_word_glyph(column, 14u, uint[16](kCharN,kCharO,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharS,kCharA,kCharV,kCharE,kCharD,kCharSpace,kCharSpace));
    }
    if (column < 14u)
    {
        return get_code_glyph(uint[14](kCharS,kCharE,kCharL,kCharE,kCharC,kCharT,kCharE,kCharD,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD)[column]);
    }
    if (column == 15u)
    {
        return get_digit_glyph(min(MenuWorldSlot + 1u, 9u));
    }
    return kGlyphBlank;
}

uint get_menu_status_glyph(uint column)
{
    if (MenuStatusCode == 0u)
    {
        return kGlyphBlank;
    }
    if (column < 6u)
    {
        return get_code_glyph(uint[6](kCharS,kCharT,kCharA,kCharT,kCharU,kCharS)[column]);
    }
    if (column < 8u)
    {
        return kGlyphBlank;
    }
    uint c = column - 8u;
    if (MenuStatusCode == 1u) { return c < 8u ? get_code_glyph(uint[8](kCharS,kCharE,kCharL,kCharE,kCharC,kCharT,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 2u) { return c < 7u ? get_code_glyph(uint[7](kCharE,kCharD,kCharI,kCharT,kCharI,kCharN,kCharG)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 3u) { return c < 14u ? get_code_glyph(uint[14](kCharC,kCharO,kCharN,kCharF,kCharI,kCharR,kCharM,kCharSpace,kCharD,kCharE,kCharL,kCharE,kCharT,kCharE)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 4u) { return c < 7u ? get_code_glyph(uint[7](kCharD,kCharE,kCharL,kCharE,kCharT,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 5u) { return c < 13u ? get_code_glyph(uint[13](kCharM,kCharI,kCharS,kCharS,kCharI,kCharN,kCharG,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 6u) { return c < 6u ? get_code_glyph(uint[6](kCharL,kCharO,kCharA,kCharD,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 7u) { return c < 7u ? get_code_glyph(uint[7](kCharC,kCharR,kCharE,kCharA,kCharT,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 8u) { return c < 5u ? get_code_glyph(uint[5](kCharS,kCharA,kCharV,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 9u) { return c < 10u ? get_code_glyph(uint[10](kCharB,kCharA,kCharD,kCharSpace,kCharS,kCharE,kCharR,kCharV,kCharE,kCharR)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 10u) { return c < 9u ? get_code_glyph(uint[9](kCharC,kCharO,kCharN,kCharN,kCharE,kCharC,kCharT,kCharE,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 11u) { return c < 9u ? get_code_glyph(uint[9](kCharN,kCharO,kCharT,kCharSpace,kCharR,kCharE,kCharA,kCharD,kCharY)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 12u) { return c < 12u ? get_code_glyph(uint[12](kCharA,kCharC,kCharT,kCharI,kCharV,kCharE,kCharSpace,kCharW,kCharO,kCharR,kCharL,kCharD)[c]) : kGlyphBlank; }
    if (MenuStatusCode == 13u) { return c < 12u ? get_code_glyph(uint[12](kCharW,kCharO,kCharR,kCharL,kCharD,kCharSpace,kCharE,kCharX,kCharI,kCharS,kCharT,kCharS)[c]) : kGlyphBlank; }
    return c < 6u ? get_code_glyph(uint[6](kCharF,kCharA,kCharI,kCharL,kCharE,kCharD)[c]) : kGlyphBlank;
}
