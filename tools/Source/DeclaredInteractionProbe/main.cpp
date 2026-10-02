#include "../../../octaryn-client/Source/Ui/DeclaredScreen/DeclaredDocumentUi.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace {
using octaryn::client::ui::DeclaredDocumentUi;
unsigned checks{};
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
struct CpuRenderer final : Rml::RenderInterface {
  struct Geometry {std::vector<Rml::Vertex> vertices;std::vector<int> indices;};
  struct Draw {Rml::CompiledGeometryHandle handle;Rml::Vector2f translation;Rml::Matrix4f transform;Rml::TextureHandle texture;};
  std::map<Rml::CompiledGeometryHandle,Geometry> geometries;
  std::vector<Rml::CompiledGeometryHandle> released;
  std::vector<Rml::String> textures;
  std::map<Rml::TextureHandle,Rml::String> texture_sources;
  std::vector<Draw> draws;
  Rml::Matrix4f transform=Rml::Matrix4f::Identity();
  Rml::CompiledGeometryHandle next=1;
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,Rml::Span<const int> indices) override {
    const auto handle=next++;geometries.emplace(handle,Geometry{{vertices.begin(),vertices.end()},{indices.begin(),indices.end()}});return handle;
  }
  void RenderGeometry(Rml::CompiledGeometryHandle handle,Rml::Vector2f translation,Rml::TextureHandle texture) override {draws.push_back({handle,translation,transform,texture});}
  void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override {released.push_back(handle);}
  Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,const Rml::String& source) override {
    textures.push_back(source);const auto handle=Rml::TextureHandle(textures.size());texture_sources.emplace(handle,source);dimensions={64,64};return handle;
  }
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i) override {return 1;}
  void ReleaseTexture(Rml::TextureHandle) override {}
  void EnableScissorRegion(bool) override {}
  void SetScissorRegion(Rml::Rectanglei) override {}
  void SetTransform(const Rml::Matrix4f* value) override {transform=value?*value:Rml::Matrix4f::Identity();}
};
struct CpuSystem final : Rml::SystemInterface {
  double elapsed=100;
  double GetElapsedTime() override {return elapsed;}
  bool LogMessage(Rml::Log::Type,const Rml::String& message) override {
    std::fprintf(stderr,"rml_fixture %s\n",message.c_str());return true;
  }
};
std::string declaration() {
  const std::string markup=R"(<rml><head><style>
    body {margin:0; background-color:transparent;}
    button {position:absolute; width:100px; height:40px; padding:0; border-width:0;}
    #a {left:100px;top:100px;} #b {left:100px;top:160px;}
    #disabled {left:100px;top:220px;}
    #field {position:absolute;width:80px;height:30px;pointer-events:none;image-color:#102030ff;}
    #b:hover #field, #b:focus #field {image-color:#ff00ffff;}
    .passive {position:absolute;width:100px;height:40px;pointer-events:none;}
    @spritesheet fixture {src:asset:fixture;resolution:1x;glyph:0px 0px 8px 12px;}
    </style></head><body><button id="a"><div id="visual" style="position:absolute;width:100px;height:40px;"/><div class="passive"/></button>
    <button id="b"><div id="field"/></button>
    <div disabled=""><button id="disabled"/></div></body></rml>)";
  std::string encoded;check(!glz::write_json(markup,encoded),"fixture markup serialization");
  return "{\"version\":2,\"screen_id\":\"fixture.ui\",\"model\":\"declared_document\",\"modal\":true,"
      "\"logical_width\":1280,\"logical_height\":720,\"markup\":"+encoded+
      R"(,"fields":[{"id":"label","element":"field","kind":"bitmap_text","font":"source_font"}],
      "bitmap_fonts":[{"id":"source_font","height":18,"glyphs":[{"codepoint":65,"sprite":"glyph",
      "width":8,"height":12,"advance":10,"bearing_x":0,"bearing_y":0}]}],
      "actions":[{"id":"fixture.ui.a","element":"a"},{"id":"fixture.ui.b","element":"b"},
      {"id":"fixture.ui.disabled","element":"disabled"}]})";
}
void layout(Rml::Context& context,DeclaredDocumentUi& ui) {
  ui.fit(context);context.Update();if(ui.sync_selection(context))context.Update();
  if(ui.reflow())context.Update();context.Render();
}
void move(Rml::Context& context,DeclaredDocumentUi& ui,int x,int y) {
  context.ProcessMouseMove(x,y,0);ui.pointer_move(context);layout(context,ui);
}
void key(Rml::Context& context,DeclaredDocumentUi& ui,Rml::Input::KeyIdentifier key) {
  ui.keyboard_input(context);context.ProcessKeyDown(key,0);layout(context,ui);
}
void contracts(Rml::Context& context) {
  DeclaredDocumentUi ui;
  check(ui.present(context,declaration(),R"({"label":"A"})"),"production declaration rejected");
  layout(context,ui);
  auto* doc=context.GetDocument(0);check(doc!=nullptr,"document missing");
  auto* a=doc->GetElementById("a");auto* b=doc->GetElementById("b");
  auto* disabled=doc->GetElementById("disabled");auto* field=doc->GetElementById("field");
  check(a->GetComputedValues().tab_index()==Rml::Style::TabIndex::Auto,"generic declared action not keyboard focusable");
  check(disabled->GetComputedValues().focus()==Rml::Style::Focus::None,"disabled ancestor remained focusable");
  move(context,ui,90,90);
  auto* visual=doc->GetElementById("visual");
  check(context.GetHoverElement()==visual,"passive overlay intercepted transformed pointer");
  check(context.GetFocusElement()==a && a->IsPseudoClassSet("hover") && a->IsPseudoClassSet("focus"),"pointer selection did not focus action");
  context.ProcessMouseButtonDown(0,0);context.ProcessMouseButtonUp(0,0);
  std::string action;check(ui.poll(action) && action=="fixture.ui.a","actual pointer click not routed once");
  check(!ui.poll(action),"click duplicated");
  check(context.GetFocusElement()==a,"visual child click stranded focus outside declared action");
  key(context,ui,Rml::Input::KI_RETURN);
  check(ui.poll(action) && action=="fixture.ui.a","Enter after visual-child click did not activate action");
  ui.pointer_position(context,90,135,0);
  context.ProcessMouseButtonDown(0,0);context.ProcessMouseButtonUp(0,0);
  check(ui.poll(action) && action=="fixture.ui.b","click position used stale previous mouse motion");
  move(context,ui,90,135);
  check(context.GetFocusElement()==b && b->IsPseudoClassSet("hover"),"pointer selection did not move to second action");
  check(!a->IsPseudoClassSet("focus") && !a->IsPseudoClassSet("hover"),"stale clicked row remained highlighted");
  check(field->GetChild(0)->GetComputedValues().image_color()==Rml::Colourb(255,0,255,255),"source hover tint not applied to bitmap glyph");
  auto* unchanged=field->GetChild(0);layout(context,ui);
  check(field->GetChild(0)==unchanged,"unchanged bitmap field rebuilt");
  key(context,ui,Rml::Input::KI_UP);
  check(context.GetFocusElement()==a,"up did not navigate source action positions");
  check(!b->IsPseudoClassSet("hover") && !b->IsPseudoClassSet("focus"),"keyboard selection retained pointer highlight");
  check(field->GetChild(0)->GetComputedValues().image_color()==Rml::Colourb(16,32,48,255),"bitmap idle tint not restored");
  key(context,ui,Rml::Input::KI_DOWN);check(context.GetFocusElement()==b,"down did not navigate");
  key(context,ui,Rml::Input::KI_RETURN);check(ui.poll(action) && action=="fixture.ui.b","Enter did not activate declared action");
  check(!ui.poll(action),"Enter duplicated");
  key(context,ui,Rml::Input::KI_TAB);check(context.GetFocusElement()==a,"Tab did not skip disabled ancestor and wrap");
  key(context,ui,Rml::Input::KI_SPACE);check(ui.poll(action) && action=="fixture.ui.a","Space did not activate declared action");
  move(context,ui,90,180);check(context.GetFocusElement()!=disabled,"disabled target received pointer focus");
  context.ProcessMouseButtonDown(0,0);context.ProcessMouseButtonUp(0,0);
  check(!ui.poll(action),"disabled ancestor admitted pointer activation");
  disabled->Click();check(!ui.poll(action),"programmatic click bypassed disabled ancestor guard");
  move(context,ui,90,90);move(context,ui,300,300);
  check(!a->IsPseudoClassSet("focus") && !a->IsPseudoClassSet("hover"),"backdrop retained stale selection");
  context.SetDimensions({1920,1080});layout(context,ui);move(context,ui,180,180);
  check(context.GetHoverElement()==visual,"resized transformed hit test missed action");
  context.SetDimensions({1920,720});layout(context,ui);move(context,ui,440,120);
  check(context.GetHoverElement()==visual,"letterboxed transformed hit test missed action");
  check(ui.hide("fixture.ui"),"hide failed");context.Update();
  check(!ui.modal() && !ui.poll(action),"closed modal retained capture or actions");
}
std::string plain_declaration(const std::string& markup,const std::vector<octaryn::client::ui::DocumentBinding>& actions) {
  std::string encoded_markup,encoded_actions;
  check(!glz::write_json(markup,encoded_markup) && !glz::write_json(actions,encoded_actions),"plain fixture serialization");
  return "{\"version\":2,\"screen_id\":\"fixture.bounds\",\"model\":\"declared_document\","
      "\"modal\":true,\"logical_width\":1280,\"logical_height\":720,\"markup\":"+encoded_markup+",\"actions\":"+encoded_actions+"}";
}
void nested(Rml::Context& context) {
  DeclaredDocumentUi ui;
  const std::string markup=R"(<rml><head><style>
    body {margin:0;} button {position:absolute;padding:0;border-width:0;}
    #row0 {left:100px;top:100px;width:250px;height:50px;nav-down:#row1;nav-right:#next0;nav-left:#prev0;}
    #row1 {left:100px;top:200px;width:250px;height:50px;nav-up:#row0;}
    #prev0 {left:10px;top:10px;width:30px;height:30px;nav-down:#row1;nav-right:#next0;}
    #next0 {left:200px;top:10px;width:30px;height:30px;nav-down:#row1;nav-left:#prev0;}
    .state {display:none;position:absolute;pointer-events:none;width:250px;height:50px;}
    #row0:focus .legacy_row {display:block;} #next0:focus .legacy_child {display:block;}
    #row0.active_declared_action .row_state {display:block;}
    #next0.active_declared_action .child_state {display:block;}
    </style></head><body><button id="row0"><div id="row_state" class="state row_state"/>
    <div id="legacy_row" class="state legacy_row"/><button id="prev0"/>
    <button id="next0"><div id="child_state" class="state child_state"/>
    <div id="legacy_child" class="state legacy_child"/></button></button>
    <button id="row1"/></body></rml>)";
  const std::vector<octaryn::client::ui::DocumentBinding> actions{
      {"fixture.bounds.row0","row0"},{"fixture.bounds.prev","prev0"},
      {"fixture.bounds.next","next0"},{"fixture.bounds.row1","row1"}};
  check(ui.present(context,plain_declaration(markup,actions),"{}"),"nested action admission");layout(context,ui);
  auto* doc=context.GetDocument(0);auto* row0=doc->GetElementById("row0");auto* row1=doc->GetElementById("row1");
  auto* next=doc->GetElementById("next0");auto* prev=doc->GetElementById("prev0");
  move(context,ui,315,125);check(context.GetFocusElement()==next,"nested exact hit target lost to row");
  check(row0->IsPseudoClassSet("focus") && next->IsPseudoClassSet("focus"),"ordinary Rml ancestor focus chain suppressed");
  check(doc->GetElementById("legacy_row")->IsVisible() && doc->GetElementById("legacy_child")->IsVisible(),"legacy focus selectors did not reproduce duplicate source snapshots");
  check(!doc->GetElementById("row_state")->IsVisible() && doc->GetElementById("child_state")->IsVisible(),"exact active-action selectors displayed overlapping snapshots");
  check(!row0->IsClassSet("active_declared_action") && next->IsClassSet("active_declared_action"),"selection class applied to ancestor and child");
  check(!ui.sync_selection(context),"unchanged selection dirtied layout");
  context.ProcessMouseButtonDown(0,0);context.ProcessMouseButtonUp(0,0);
  std::string action;check(ui.poll(action) && action=="fixture.bounds.next" && !ui.poll(action),"nested click routed parent instead of exact child");
  key(context,ui,Rml::Input::KI_DOWN);check(context.GetFocusElement()==row1,"explicit child-to-next-row route lost");
  key(context,ui,Rml::Input::KI_UP);check(context.GetFocusElement()==row0,"explicit row route lost");
  check(doc->GetElementById("row_state")->IsVisible() && !doc->GetElementById("child_state")->IsVisible(),"row selection did not replace child snapshot");
  key(context,ui,Rml::Input::KI_LEFT);check(context.GetFocusElement()==prev,"explicit source left route overwritten");
  key(context,ui,Rml::Input::KI_RETURN);check(ui.poll(action) && action=="fixture.bounds.prev","nested keyboard activated wrong target");
  move(context,ui,315,125);
  check(ui.hide("fixture.bounds"),"first menu swap hide");context.Update();
  check(ui.present(context,plain_declaration(markup,actions),"{}"),"menu swap presentation");layout(context,ui);
  doc=context.GetDocument(0);next=doc->GetElementById("next0");row0=doc->GetElementById("row0");
  check(context.GetHoverElement()==next && context.GetFocusElement()==next,"stationary pointer did not select new menu target");
  key(context,ui,Rml::Input::KI_LEFT);prev=doc->GetElementById("prev0");
  check(context.GetFocusElement()==prev && context.GetHoverElement()==nullptr,"keyboard navigation failed to become authoritative");
  layout(context,ui);check(context.GetFocusElement()==prev,"stationary pointer stole keyboard selection");
  move(context,ui,315,125);context.SetDimensions({960,540});layout(context,ui);layout(context,ui);
  check(!next->IsClassSet("active_declared_action"),"logical fit moved pointer target but retained stale selection");
  context.SetDimensions({1280,720});layout(context,ui);layout(context,ui);
  check(next->IsClassSet("active_declared_action"),"stationary pointer target did not reconcile restored logical fit");
  check(ui.hide("fixture.bounds"),"nested hide");context.Update();
}
void bounds(Rml::Context& context) {
  DeclaredDocumentUi ui;std::vector<octaryn::client::ui::DocumentBinding> actions;
  std::string markup="<rml><head/><body>";
  for(unsigned index=0;index<64;++index) {
    const auto id="control_"+std::to_string(index);actions.push_back({"fixture.bounds."+id,id});
    markup+="<button id=\""+id+"\"/>";
  }
  markup+="</body></rml>";
  check(ui.present(context,plain_declaration(markup,actions),"{}"),"exact 64-action boundary rejected");layout(context,ui);
  auto* target=context.GetDocument(0)->GetElementById("control_0");
  for(unsigned index=0;index<70;++index)target->Click();
  std::string action;unsigned count=0;while(ui.poll(action))++count;
  check(count==64,"pending action queue not bounded to64");
  check(ui.hide("fixture.bounds"),"bounds hide");context.Update();
  actions.push_back({"fixture.bounds.excess","excess"});
  markup.insert(markup.find("</body>"),"<button id=\"excess\"/>");
  check(!ui.present(context,plain_declaration(markup,actions),"{}"),"65-action declaration admitted");
  check(context.GetNumDocuments()==0 && !ui.poll(action),"rejected declaration leaked document or actions");
}
void wrapped_fields(Rml::Context& context) {
  auto declared=declaration();
  const auto replace=[&](const std::string& before,const std::string& after) {
    const auto at=declared.find(before);check(at!=std::string::npos,"wrap fixture source differs");
    declared.replace(at,before.size(),after);
  };
  replace("\"font\":\"source_font\"","\"font\":\"source_font\",\"wrap_width\":30");
  replace("\"height\":18,\"glyphs\"","\"height\":18,\"line_height\":21,\"glyphs\"");
  replace("\"bearing_y\":0}]}","\"bearing_y\":0},{\"codepoint\":32,\"sprite\":\"glyph\",\"width\":0,\"height\":0,\"advance\":4,\"bearing_x\":0,\"bearing_y\":0}]}");
  DeclaredDocumentUi ui;
  check(ui.present(context,declared,R"({"label":"AA AA"})"),"wrapped production field rejected");layout(context,ui);
  auto* field=context.GetDocument(0)->GetElementById("field");
  check(field->GetNumChildren()==4,"soft break rendered a separator or lost source glyphs");
  check(field->GetChild(0)->GetRelativeOffset(Rml::BoxArea::Border).y==0 &&
      field->GetChild(2)->GetRelativeOffset(Rml::BoxArea::Border).y==21,
      "production Rml field lost authored line height");
  auto* cached=field->GetChild(2);
  check(ui.present(context,declared,R"({"label":"AA AA"})"),"unchanged wrapped update rejected");layout(context,ui);
  check(field->GetChild(2)==cached,"unchanged wrapped field regenerated geometry");
  check(ui.hide("fixture.ui"),"wrap fixture hide");context.Update();
  declared.replace(declared.find("\"wrap_width\":30"),15,"\"wrap_width\":-1");
  check(!ui.present(context,declared,R"({"label":"AA AA"})") && context.GetNumDocuments()==0,
      "native admission accepted invalid wrap width or leaked a document");
}
void meshes(Rml::Context& context,CpuRenderer& renderer) {
  const std::string markup=R"(<rml><head><style>body {margin:0;}
    #mesh {position:absolute;left:30px;top:40px;width:100px;height:80px;
      transform:translate(7px,11px);opacity:.5;image-color:#804020ff;}
    </style></head><body><div id="mesh"/></body></rml>)";
  std::string encoded;check(!glz::write_json(markup,encoded),"mesh markup serialization");
  const auto declared="{\"version\":2,\"screen_id\":\"fixture.mesh\",\"model\":\"declared_document\",\"markup\":"+encoded+
      R"(,"meshes":[{"element":"mesh","texture":"fixture-skew.png","wrap":true,
      "vertices":[{"x":2,"y":3,"u":0.25,"v":-0.5},{"x":91,"y":7,"u":2.5,"v":0.125},
      {"x":73,"y":69,"u":1.75,"v":1.5},{"x":-5,"y":54,"u":-0.25,"v":0.75}],"indices":[0,2,1,0,3,2]}]})";
  const auto mesh_handle=[&] {
    Rml::CompiledGeometryHandle result{};
    for(const auto& draw:renderer.draws) {
      const auto& geometry=renderer.geometries.at(draw.handle);
      if(geometry.vertices.size()==4 && geometry.vertices[0].tex_coord==Rml::Vector2f(.25f,-.5f))result=draw.handle;
    }
    return result;
  };
  DeclaredDocumentUi ui;renderer.draws.clear();
  check(ui.present(context,declared,"{}"),"production skew mesh admission failed");layout(context,ui);
  auto* document=context.GetDocument(0);auto* target=document->GetElementById("mesh");
  check(target && target->GetNumChildren()==1,"mesh host did not retain its injected geometry element");
  auto handle=mesh_handle();check(handle!=0,"mesh did not reach production Rml geometry compilation");
  const auto& geometry=renderer.geometries.at(handle);
  const std::array<Rml::Vector2f,4> positions{{{2,3},{91,7},{73,69},{-5,54}}};
  const std::array<Rml::Vector2f,4> uv{{{.25f,-.5f},{2.5f,.125f},{1.75f,1.5f},{-.25f,.75f}}};
  check(geometry.indices==std::vector<int>({0,2,1,0,3,2}),"source mesh triangle order changed");
  for(unsigned index=0;index<4;++index)check(geometry.vertices[index].position==positions[index] &&
      geometry.vertices[index].tex_coord==uv[index],"source skew mesh position or UV was rescaled or normalized");
  const auto tint=Rml::Colourb(128,64,32,255).ToPremultiplied(.5f);
  check(std::all_of(geometry.vertices.begin(),geometry.vertices.end(),[&](const auto& vertex){return vertex.colour==tint;}),
      "mesh lost parent palette tint or inherited CSS opacity");
  check(std::find(renderer.textures.begin(),renderer.textures.end(),"octaryn-ui-wrap:fixture-skew.png")!=renderer.textures.end(),
      "explicit source repeat sampling did not reach the renderer texture boundary");
  const auto draw=std::find_if(renderer.draws.begin(),renderer.draws.end(),[&](const auto& value){return value.handle==handle;});
  const auto moved=draw->transform*Rml::Vector4f(draw->translation.x,draw->translation.y,0,1);
  check(std::abs(moved.x-37)<.001f && std::abs(moved.y-51)<.001f,"mesh did not inherit authored CSS transform and placement");
  const auto count=renderer.geometries.size();auto* child=target->GetChild(0);renderer.draws.clear();
  check(ui.present(context,declared,"{}"),"unchanged mesh publication failed");layout(context,ui);
  check(context.GetDocument(0)==document && target->GetChild(0)==child && renderer.geometries.size()==count && mesh_handle()==handle,
      "unchanged mesh publication reloaded its document or rebuilt geometry");
  target->SetProperty("opacity",".25");renderer.draws.clear();layout(context,ui);
  const auto recolored=mesh_handle();check(recolored!=0 && recolored!=handle,"CSS opacity update did not recolor retained mesh");
  check(renderer.geometries.at(recolored).vertices[0].colour==Rml::Colourb(128,64,32,255).ToPremultiplied(.25f),
      "updated mesh opacity did not reach premultiplied vertex color");
  check(std::find(renderer.released.begin(),renderer.released.end(),handle)!=renderer.released.end(),"replaced mesh geometry was not released");
  check(ui.hide("fixture.mesh"),"mesh hide failed");context.Update();
  check(context.GetNumDocuments()==0 && std::find(renderer.released.begin(),renderer.released.end(),recolored)!=renderer.released.end(),
      "hidden mesh document retained native geometry");
  renderer.draws.clear();
  {DeclaredDocumentUi scoped;check(scoped.present(context,declared,"{}"),"scoped mesh publication failed");layout(context,scoped);handle=mesh_handle();}
  context.Update();check(handle!=0 && context.GetNumDocuments()==0 &&
      std::find(renderer.released.begin(),renderer.released.end(),handle)!=renderer.released.end(),"UI destruction retained mesh document or geometry");
}
void height_canvas(Rml::Context& context,CpuRenderer& renderer,CpuSystem& system) {
  const std::string markup=R"(<rml><head><style>body {margin:0;width:100%;height:100%;}
    #picture {position:absolute;left:25%;top:10%;width:50%;height:100px;opacity:.2;}
    #matte {position:absolute;left:10%;top:80%;width:80%;padding-top:12px;padding-bottom:12px;}
    #bar {position:absolute;left:0px;top:0px;width:100%;height:100%;}
    #tip {position:relative;display:block;width:4%;pointer-events:none;}
    @spritesheet fit_font {src:fixture-height-font.png;glyph:0px 0px 8px 12px;}
    </style></head><body><div id="picture"/><div id="matte"><div id="bar"/><div id="tip"/></div></body></rml>)";
  std::string encoded;check(!glz::write_json(markup,encoded),"height canvas markup serialization");
  auto declared="{\"version\":2,\"screen_id\":\"fixture.height\",\"model\":\"declared_document\",\"markup\":"+encoded+
      R"(,"logical_width":1280,"logical_height":720,"fit_mode":"height",
      "fields":[{"id":"tip","element":"tip","kind":"bitmap_text","font":"fit_font","wrap_to_element":true,"fit_text_height":true}],
      "bitmap_fonts":[{"id":"fit_font","height":18,"line_height":21,"measured_height":14,
      "glyphs":[{"codepoint":65,"sprite":"glyph","width":8,"height":12,"advance":10},
      {"codepoint":32,"sprite":"glyph","advance":4}]}],
      "meshes":[{"element":"picture","texture":"fixture-height-picture.png",
      "vertices":[{"x":0,"y":0,"u":0.3,"v":0.6},{"x":40,"y":0,"u":1,"v":0},
      {"x":0,"y":40,"u":0,"v":1}],"indices":[0,1,2]}]})";
  DeclaredDocumentUi ui;context.SetDimensions({1280,720});
  check(ui.present(context,declared,R"({"tip":"AA AA"})"),"height canvas publication failed");layout(context,ui);
  auto* document=context.GetDocument(0);auto* picture=document->GetElementById("picture");
  auto* tip=document->GetElementById("tip");auto* matte=document->GetElementById("matte");auto* mesh=picture->GetChild(0);
  check(tip->GetClientHeight()==35 && matte->GetClientHeight()==59,"flow matte did not follow measured first line plus wrapped line advance and padding");
  check(document->GetElementById("bar")->GetClientHeight()==59,"absolute matte background failed to include autoheight padding");
  check(picture->Animate("opacity",Rml::Property(1.f,Rml::Unit::NUMBER),10,Rml::Tween(Rml::Tween::Linear)),"retained timeline fixture failed");
  const auto start=system.elapsed;layout(context,ui);
  for(const auto size:std::array<Rml::Vector2i,3>{{{1280,960},{2560,1080},{1280,720}}}) {
    for(unsigned frame=0;frame<50;++frame){system.elapsed+=.02;layout(context,ui);}
    context.SetDimensions(size);renderer.draws.clear();layout(context,ui);
    check(context.GetDocument(0)==document && picture->GetChild(0)==mesh,"resize replaced source document or mesh element");
    check(std::abs(picture->GetComputedValues().opacity()-(.2+.8*(system.elapsed-start)/10))<.002,
        "resize reset retained source animation timeline");
    const auto physical_origin=[](const CpuRenderer::Draw& draw,const Rml::Vertex& vertex) {
      return draw.transform*Rml::Vector4f(draw.translation.x+vertex.position.x,draw.translation.y+vertex.position.y,0,1);
    };
    const auto picture_draw=std::find_if(renderer.draws.begin(),renderer.draws.end(),[&](const auto& draw) {
      return renderer.texture_sources.contains(draw.texture) && renderer.texture_sources.at(draw.texture)=="fixture-height-picture.png";
    });
    check(picture_draw!=renderer.draws.end(),"resized source picture did not render");
    const auto picture_position=physical_origin(*picture_draw,renderer.geometries.at(picture_draw->handle).vertices[0]);
    const double layout_pixel=size.y/720.;
    check(std::abs(picture_position.x-size.x*.25)<=layout_pixel && std::abs(picture_position.y-size.y*.1)<=layout_pixel,
        "source percentage picture position changed relative to viewport");
    const auto tip_draw=std::find_if(renderer.draws.begin(),renderer.draws.end(),[&](const auto& draw) {
      return renderer.texture_sources.contains(draw.texture) && renderer.texture_sources.at(draw.texture)=="fixture-height-font.png";
    });
    check(tip_draw!=renderer.draws.end(),"resized source tip did not render");
    const auto tip_position=physical_origin(*tip_draw,renderer.geometries.at(tip_draw->handle).vertices[0]);
    check(std::abs(tip_position.x-size.x*.1)<=layout_pixel && std::abs(tip_position.y-(size.y*.8+12*size.y/720.))<=layout_pixel,
        "source tip or matte top ratio changed across viewport aspects");
    check(ui.present(context,declared,R"({"tip":"AA AA"})") && context.GetDocument(0)==document,"unchanged height canvas reloaded its document");
    const double expected_height=size.x==2560?14:35;
    check(tip->GetClientHeight()==expected_height && matte->GetClientHeight()==expected_height+24,
        "element-width wrapping or matte height failed to reevaluate across viewport aspects");
  }
  check(ui.present(context,declared,R"({"tip":"A"})"),"shorter measured tip update failed");layout(context,ui);
  check(tip->GetClientHeight()==14 && matte->GetClientHeight()==38,"matte failed to shrink with native first-line height");
  check(ui.present(context,declared,R"({"tip":""})"),"empty measured tip update failed");layout(context,ui);
  check(tip->GetClientHeight()==0 && matte->GetClientHeight()==24,"empty tip retained stale text height");
  check(ui.hide("fixture.height"),"height canvas hide failed");context.Update();
  declared.replace(declared.find("\"fit_mode\":\"height\""),19,"\"fit_mode\":\"stretch\"");
  check(!ui.present(context,declared,"{}") && context.GetNumDocuments()==0,"native admission accepted invalid canvas fitting");
}
}
int main() {
  CpuRenderer renderer;CpuSystem system;Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);
  if(!Rml::Initialise())return 2;
  int result=0;
  try {auto* context=Rml::CreateContext("authored",{960,540});check(context!=nullptr,"context creation");
    contracts(*context);context->SetDimensions({1280,720});nested(*context);bounds(*context);wrapped_fields(*context);meshes(*context,renderer);
    height_canvas(*context,renderer,system);Rml::RemoveContext("authored");
    std::printf("declared_interaction checks=%u passed=1 production_rml=1 gpu=0 os_input=0\n",checks);
  } catch(const std::exception& error) {std::fprintf(stderr,"declared_interaction checks=%u failed=%s\n",checks,error.what());result=1;}
  Rml::Shutdown();Rml::SetRenderInterface(nullptr);Rml::SetSystemInterface(nullptr);return result;
}
