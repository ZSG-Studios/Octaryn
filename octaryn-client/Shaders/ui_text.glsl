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
