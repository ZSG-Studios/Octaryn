#include "../../../octaryn-client/Source/Ui/DeclaredScreen/DeclaredBitmapText.h"
#include "../../../octaryn-client/Source/Ui/DeclaredScreen/DeclaredDocumentLayout.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
using namespace octaryn::client::ui;
unsigned checks{};
void check(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
BitmapFont fixture() {
  return {"original_fixture",18,{{65,"glyph_a",8,12,10,1,-2},{66,"glyph_b",6,10,7,-1,0},
      {32,"glyph_space",0,0,4,0,0},{0x20ac,"glyph_euro",9,12,11,0,0},{0x1f600,"glyph_authored_unicode",8,12,10,0,0}}};
}
void positioning() {
  auto font=fixture();DocumentBinding field;std::string output="stale";
  check(bitmap_markup(font,field,"AB",100,output),"authored glyphs rejected");
  check(output.find("left:1px;top:-2px;width:8px;height:12px")!=std::string::npos,"render metrics changed");
  check(output.find("left:9px;top:0px;width:6px;height:10px")!=std::string::npos,"advance or bearing lost");
  check(output.find("stale")==std::string::npos,"successful update appended to old markup");
  field.align="right";field.scale=2;
  check(bitmap_markup(font,field,"AB\nA",100,output),"multiline text rejected");
  check(output.find("left:68px;top:-4px")!=std::string::npos,"first line right alignment used another line width");
  check(output.find("left:82px;top:32px")!=std::string::npos,"second line baseline or alignment changed");
  field.align="center";field.scale=1;
  check(bitmap_markup(font,field,"AB\nA",100,output),"centered multiline rejected");
  check(output.find("left:42.5px;top:-2px")!=std::string::npos,"first line centering changed");
  check(output.find("left:46px;top:16px")!=std::string::npos,"second line centering changed");
  check(bitmap_markup(font,field,"A A",100,output),"zero-area source space rejected");
  check(output.find("sprite=\"glyph_space\"")==std::string::npos,"space created invented visible art");
  check(bitmap_markup(font,field,"",100,output) && output.empty(),"empty text did not clear the field");
  check(bitmap_markup(font,field,"A",100,output,BitmapTint{0,128,255,64}),"explicit tint rejected");
  check(output.find("image-color:#0080ff40;")!=std::string::npos,"source RGB or alpha tint changed");
  check(output.find("inherit")==std::string::npos,"glyph tint depends on non-inherited CSS property");
  check(bitmap_color(BitmapTint{255,0,16,255})=="#ff0010ff","tint channel serialization changed");
}
void admission() {
  auto font=fixture();DocumentBinding field;std::string output;
  check(bitmap_markup(font,field,"\xe2\x82\xac\xf0\x9f\x98\x80",100,output),"authored Unicode mapping rejected");
  check(output.find("glyph_euro")!=std::string::npos && output.find("glyph_authored_unicode")!=std::string::npos,"Unicode selected wrong glyph");
  const std::string invalid[]={"Z","A\t","A\r",std::string("A\0B",3),"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80",
      "\xe2\x82","\x80","\xe2X\xac",std::string(257,'A')};
  for(const auto& text:invalid) {
    output="preserved";check(!bitmap_markup(font,field,text,100,output),"unsupported text was accepted");
    check(output=="preserved","rejected text partially mutated output");
  }
  check(bitmap_markup(font,field,std::string(256,'A'),100,output),"256 glyph bound rejected");
  std::size_t offset=0;unsigned code{};check(!bitmap_character("",offset,code),"empty UTF8 read escaped bounds");
  auto changed=font;changed.glyphs.push_back(changed.glyphs[0]);check(!bitmap_font_valid(changed),"duplicate character admitted");
  changed=font;changed.glyphs[0].sprite="glyph\" bad";check(!bitmap_font_valid(changed),"sprite markup injection admitted");
  changed=font;changed.glyphs[0].advance=-1;check(!bitmap_font_valid(changed),"negative advance admitted");
  changed=font;changed.glyphs[0].width=std::numeric_limits<double>::infinity();check(!bitmap_font_valid(changed),"nonfinite metric admitted");
  changed=font;changed.glyphs[0].codepoint=0xd800;check(!bitmap_font_valid(changed),"surrogate font mapping admitted");
  changed=font;changed.height=0;check(!bitmap_font_valid(changed),"zero line height admitted");
  changed=font;changed.glyphs.resize(257);check(!bitmap_font_valid(changed),"font glyph budget ignored");
  for(double width:{-1.,65537.,std::numeric_limits<double>::quiet_NaN()})
    check(!bitmap_markup(font,field,"A",width,output),"invalid available width admitted");
  field.align="invented";check(!bitmap_markup(font,field,"A",100,output),"unknown alignment admitted");
  field.align="left";field.scale=0;check(!bitmap_markup(font,field,"A",100,output),"invalid scale admitted");
}
void caching() {
  check(bitmap_cached("AB","AB",100,100),"unchanged field did not skip mutation");
  check(!bitmap_cached("AB","A",100,100),"changed field was skipped");
  check(!bitmap_cached("AB","AB",100,200),"resize kept stale aligned glyph geometry");
  check(!bitmap_cached("AB","AB",100,std::numeric_limits<double>::quiet_NaN()),"nonfinite resize was cached");
  const BitmapTint tint{255,128,0,255};
  check(bitmap_cached("AB","AB",100,100,tint,tint),"unchanged source tint was not cached");
  check(!bitmap_cached("AB","AB",100,100,tint,bitmap_white),"changed source tint kept stale glyph color");
  check(!bitmap_cached("AB","AB",100,100,tint,BitmapTint{255,128,0,128}),"changed source alpha kept stale glyph color");
}
void wrapping() {
  auto font=fixture();font.line_height=21;DocumentBinding field;field.wrap_width=34;
  std::string output;
  check(bitmap_markup(font,field,"AB AB",100,output),"word-wrapped field rejected");
  check(output.find("left:1px;top:19px")!=std::string::npos &&
      output.find("left:9px;top:21px")!=std::string::npos,"word boundary or authored line advance lost");
  field.wrap_width=38;
  check(bitmap_markup(font,field,"AB AB",100,output),"exact-fit words rejected");
  check(output.find("left:22px;top:-2px")!=std::string::npos &&
      output.find("top:19px")==std::string::npos,"word that fits exactly was wrapped");
  field.wrap_width=34;field.align="right";
  check(bitmap_markup(font,field,"AB  AB\nA",100,output),"explicit and soft breaks rejected");
  check(output.find("left:84px;top:-2px")!=std::string::npos &&
      output.find("left:84px;top:19px")!=std::string::npos &&
      output.find("left:91px;top:40px")!=std::string::npos,"soft-break separators affected line alignment or explicit baseline");
  field.align="left";field.scale=2;field.wrap_width=68;
  check(bitmap_markup(font,field,"AB AB",100,output) && output.find("top:38px")!=std::string::npos,
      "wrapping ignored scaled advances or bearings");
  field.scale=1;field.wrap_width=5;
  check(bitmap_markup(font,field,"AB",100,output) && output.find("left:9px;top:0px")!=std::string::npos,
      "unbreakable word was silently split");
  field.wrap_width=0;
  check(bitmap_markup(font,field,"AB AB",100,output) && output.find("left:22px;top:-2px")!=std::string::npos,
      "default wrapping changed existing fields");
  for(double width:{-1.,16385.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
    field.wrap_width=width;output="unchanged";
    check(!bitmap_markup(font,field,"AB AB",100,output) && output=="unchanged","invalid wrap width admitted or partially published");
  }
  font.line_height=std::numeric_limits<double>::quiet_NaN();
  check(!bitmap_font_valid(font),"invalid line metric admitted");
}
void measured_fields() {
  auto font=fixture();font.line_height=21;font.measured_height=14;
  DocumentBinding field;field.fit_text_height=true;field.wrap_to_element=true;
  std::string output;double height=-1;
  check(bitmap_markup(font,field,"AB AB",34,output,bitmap_white,&height) && height==35,
      "element-width field lost native measured height or authored extra-line advance");
  check(bitmap_markup(font,field,"AB AB",38,output,bitmap_white,&height) && height==14,
      "element-width field did not unwrap at exact fit");
  field.scale=2;
  check(bitmap_markup(font,field,"AB AB",68,output,bitmap_white,&height) && height==70,
      "measured field ignored source scale");
  check(bitmap_markup(font,field,"",68,output,bitmap_white,&height) && height==0,"empty fitted text retained height");
  output="preserved";height=-1;
  check(!bitmap_markup(font,field,"Z",68,output,bitmap_white,&height) && height==-1 && output=="preserved",
      "invalid measured field partially published height or markup");
  font.measured_height=std::numeric_limits<double>::infinity();
  check(!bitmap_font_valid(font),"nonfinite measured first-line metric admitted");
}
void logical_canvas() {
  DocumentFit fit;
  check(document_canvas_valid(0,0),"legacy context-sized document rejected");
  for(const auto dimensions:std::array<std::array<unsigned,2>,5>{{{0,720},{1280,0},{63,720},{1280,4097},{4097,720}}})
    check(!document_canvas_valid(dimensions[0],dimensions[1]),"invalid logical canvas admitted");
  check(document_fit(1280,720,960,540,fit) && fit.scale==.75 && fit.left==0 && fit.top==0,"960x540 menu fit differs");
  check(document_fit(1280,720,1920,1080,fit) && fit.scale==1.5 && fit.left==0 && fit.top==0,"1920x1080 menu fit differs");
  check(document_fit(1280,720,1280,960,fit) && fit.scale==1 && fit.left==0 && fit.top==120,"tall viewport letterbox differs");
  check(document_fit(1280,720,1920,720,fit) && fit.scale==1 && fit.left==320 && fit.top==0,"wide viewport letterbox differs");
  check(document_fit(1280,720,1280,960,fit,"height") && fit.scale==4./3 && fit.width==960 && fit.height==720 &&
      fit.left==0 && fit.top==0,"height fit did not recalculate 4:3 logical width");
  check(document_fit(1280,720,2560,1080,fit,"height") && fit.scale==1.5 && std::abs(fit.width-2560/1.5)<.00001 &&
      fit.left==0 && fit.top==0,"height fit did not recalculate ultrawide logical width");
  check(document_fit(1280,720,1,1,fit) && fit.scale>0 && std::abs(fit.left)<.00001 && fit.top>=0,"minimized window fit invalid");
  fit={7,8,9};check(!document_fit(0,0,960,540,fit) && fit.scale==7,"legacy canvas changed by fitting");
  check(!document_fit(1280,720,0,540,fit) && fit.scale==7,"invalid viewport mutated output");
  check(!document_fit(1280,720,960,540,fit,"stretch") && fit.scale==7,"invalid fit mode admitted or mutated output");
}
}
int main() {
  try {positioning();admission();caching();wrapping();measured_fields();logical_canvas();std::printf("declared_bitmap_text checks=%u passed=1 gpu=0\n",checks);return 0;}
  catch(const std::exception& error) {std::fprintf(stderr,"declared_bitmap_text failed=%s\n",error.what());return 1;}
}
