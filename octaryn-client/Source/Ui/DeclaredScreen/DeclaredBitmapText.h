#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace octaryn::client::ui {
struct BitmapGlyph {
  int codepoint{};std::string sprite;
  double width{},height{},advance{},bearing_x{},bearing_y{};
};
struct BitmapFont {std::string id;double height{};std::vector<BitmapGlyph> glyphs;double line_height{},measured_height{};};
struct DocumentBinding {
  std::string id,element,kind="text",font,align="left";double scale=1,wrap_width{};
  bool fit_text_height{},wrap_to_element{};
};
using BitmapTint=std::array<unsigned char,4>;
inline constexpr BitmapTint bitmap_white{255,255,255,255};
inline std::string bitmap_color(const BitmapTint& tint) {
  constexpr char hex[]="0123456789abcdef";std::string result="#";
  for(const auto channel:tint){result+=hex[channel>>4];result+=hex[channel&15];}
  return result;
}
inline bool bitmap_identifier(const std::string& value) {
  return !value.empty() && value.size()<=128 && std::all_of(value.begin(),value.end(),[](unsigned char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.';
  });
}
inline bool bitmap_font_valid(const BitmapFont& font) {
  if(!bitmap_identifier(font.id) || !std::isfinite(font.height) || font.height<.1 || font.height>512 ||
      !std::isfinite(font.line_height) || font.line_height<0 || font.line_height>512 ||
      (font.line_height>0 && font.line_height<.1) ||
      !std::isfinite(font.measured_height) || font.measured_height<0 || font.measured_height>512 ||
      (font.measured_height>0 && font.measured_height<.1) ||
      font.glyphs.empty() || font.glyphs.size()>256)return false;
  std::vector<int> codes;
  for(const auto& glyph:font.glyphs) {
    if(glyph.codepoint<32 || glyph.codepoint>0x10ffff || (glyph.codepoint>=0xd800 && glyph.codepoint<=0xdfff) ||
        !bitmap_identifier(glyph.sprite) || !std::isfinite(glyph.width) || !std::isfinite(glyph.height) ||
        !std::isfinite(glyph.advance) || !std::isfinite(glyph.bearing_x) || !std::isfinite(glyph.bearing_y) ||
        glyph.width<0 || glyph.height<0 || glyph.advance<0 || glyph.width>16384 || glyph.height>16384 ||
        glyph.advance>16384 || std::abs(glyph.bearing_x)>16384 || std::abs(glyph.bearing_y)>16384 ||
        std::find(codes.begin(),codes.end(),glyph.codepoint)!=codes.end())return false;
    codes.push_back(glyph.codepoint);
  }
  return true;
}
inline bool bitmap_cached(const std::string& prior,const std::string& text,double prior_width,double width,
    const BitmapTint& prior_tint=bitmap_white,const BitmapTint& tint=bitmap_white) {
  return prior==text && std::isfinite(width) && width>=0 && width<=65536 && prior_width==width && prior_tint==tint;
}
inline std::string bitmap_number(double value) {
  char output[64];const auto result=std::to_chars(output,output+sizeof(output),value,std::chars_format::general,12);
  return result.ec==std::errc{}?std::string(output,result.ptr):"";
}
inline bool bitmap_character(const std::string& text,std::size_t& position,unsigned& code) {
  if(position>=text.size())return false;
  const auto first=static_cast<unsigned char>(text[position++]);
  if(first<128){code=first;return true;}
  unsigned count{};
  if(first>=0xc2 && first<=0xdf){count=1;code=first&31;}
  else if(first>=0xe0 && first<=0xef){count=2;code=first&15;}
  else if(first>=0xf0 && first<=0xf4){count=3;code=first&7;}
  else return false;
  if(position+count>text.size())return false;
  const auto minimum=count==1?128u:count==2?2048u:65536u;
  for(unsigned index=0;index<count;++index){const auto next=static_cast<unsigned char>(text[position++]);if((next&0xc0)!=0x80)return false;code=(code<<6)|(next&63);}
  return code>=minimum && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff);
}
inline bool bitmap_markup(const BitmapFont& font,const DocumentBinding& field,const std::string& text,double available,std::string& output,
    const BitmapTint& tint=bitmap_white,double* measured_height=nullptr) {
  if(!bitmap_font_valid(font) || text.size()>1024 || !std::isfinite(available) || available<0 || available>65536 ||
      (field.align!="left" && field.align!="center" && field.align!="right") ||
      !std::isfinite(field.scale) || field.scale<.1 || field.scale>8 ||
      !std::isfinite(field.wrap_width) || field.wrap_width<0 || field.wrap_width>16384)return false;
  struct Positioned {const BitmapGlyph* glyph;double x,y;std::size_t line;};
  std::vector<Positioned> glyphs;std::vector<double> lines{0};double pen=0,y=0;unsigned count=0;
  const double line_height=font.line_height?font.line_height:font.height;
  const double wrap=field.wrap_to_element?available:field.wrap_width;
  std::size_t before_spaces{};double width_before_spaces{};unsigned prior='\n';
  for(std::size_t offset=0;offset<text.size();) {
    unsigned code{};if(!bitmap_character(text,offset,code) || ++count>256)return false;
    if(code=='\n'){pen=0;y+=line_height;if(y>65536)return false;lines.push_back(0);prior=code;continue;}
    const BitmapGlyph* glyph{};
    for(const auto& candidate:font.glyphs)if(candidate.codepoint==static_cast<int>(code)){glyph=&candidate;break;}
    if(!glyph)return false;
    if(wrap>0 && code!=' ' && prior==' ' && width_before_spaces>0) {
      double word=glyph->advance;
      for(std::size_t next=offset;next<text.size();) {
        unsigned following{};if(!bitmap_character(text,next,following))return false;
        if(following==' ' || following=='\n')break;
        const auto found=std::find_if(font.glyphs.begin(),font.glyphs.end(),
            [&](const auto& candidate){return candidate.codepoint==static_cast<int>(following);});
        if(found==font.glyphs.end())return false;
        word+=found->advance;
      }
      if((pen+word)*field.scale>wrap) {
        glyphs.resize(before_spaces);lines.back()=width_before_spaces;
        pen=0;y+=line_height;if(y>65536)return false;lines.push_back(0);
      }
    }
    if(code==' ' && prior!=' '){before_spaces=glyphs.size();width_before_spaces=pen;}
    glyphs.push_back({glyph,pen+glyph->bearing_x,y+glyph->bearing_y,lines.size()-1});pen+=glyph->advance;
    if(!std::isfinite(pen) || std::abs(pen)>65536)return false;
    lines.back()=pen;
    prior=code;
  }
  std::string markup;
  for(const auto& positioned:glyphs) {
    const auto& glyph=*positioned.glyph;if(glyph.width==0 || glyph.height==0)continue;
    const auto width=lines[positioned.line]*field.scale;
    const double left=field.align=="right"?available-width:field.align=="center"?(available-width)/2:0;
    markup+="<img sprite=\""+glyph.sprite+"\" style=\"position:absolute;left:"+bitmap_number(left+positioned.x*field.scale)+
      "px;top:"+bitmap_number(positioned.y*field.scale)+"px;width:"+bitmap_number(glyph.width*field.scale)+
      "px;height:"+bitmap_number(glyph.height*field.scale)+"px;image-color:"+bitmap_color(tint)+";pointer-events:none;\"/>";
    if(markup.size()>128*1024)return false;
  }
  const double measured=text.empty()?0:((font.measured_height?font.measured_height:font.height)+(lines.size()-1)*line_height)*field.scale;
  if(field.fit_text_height && (!std::isfinite(measured) || measured>65536))return false;
  output=std::move(markup);if(measured_height)*measured_height=measured;
  return true;
}
}
