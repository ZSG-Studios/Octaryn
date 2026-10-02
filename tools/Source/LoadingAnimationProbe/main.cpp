#include "../../../octaryn-client/Source/Ui/DeclaredScreen/DeclaredDocumentUi.h"
#include <glaze/glaze.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>

struct Vertex {double x{},y{},u{},v{};};
struct MeshExpectation {std::string element;double seconds{},endpoint_degrees{};std::vector<Vertex> vertices;std::vector<int> indices;};
struct Profile {std::string declaration;double slide_seconds{5},fade_seconds{.75},initial_delay_seconds{};bool fade_starts_at_event{};std::vector<MeshExpectation> meshes;};
namespace {
unsigned checks{};
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
struct Clock final : Rml::SystemInterface {
  double now{};unsigned diagnostics{};
  double GetElapsedTime() override {return now;}
  bool LogMessage(Rml::Log::Type type,const Rml::String& text) override {
    if(type==Rml::Log::LT_WARNING || type==Rml::Log::LT_ERROR) {
      ++diagnostics;
      std::fprintf(stderr,"loading_animation_rml %s\n",text.c_str());
    }
    return true;
  }
};
struct Sink final : Rml::RenderInterface {
  unsigned next{1};std::vector<MeshExpectation> expected;
  std::map<Rml::CompiledGeometryHandle,int> matched;std::vector<unsigned> drawn;
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,Rml::Span<const int> indices) override {
    const auto handle=Rml::CompiledGeometryHandle(next++);int match=-1;
    for(unsigned m=0;m<expected.size();++m) {
      const auto& mesh=expected[m];if(vertices.size()!=mesh.vertices.size() || indices.size()!=mesh.indices.size())continue;
      bool equal=true;
      for(unsigned i=0;i<vertices.size();++i) {
        const auto& a=vertices[i];const auto& b=mesh.vertices[i];
        equal=equal && std::abs(a.position.x-b.x)<.001 && std::abs(a.position.y-b.y)<.001 &&
            std::abs(a.tex_coord.x-b.u)<.000001 && std::abs(a.tex_coord.y-b.v)<.000001;
      }
      for(unsigned i=0;i<indices.size();++i)equal=equal && indices[i]==mesh.indices[i];
      if(equal) {check(match==-1,"ambiguous source mesh geometry");match=int(m);}
    }
    matched[handle]=match;return handle;
  }
  void RenderGeometry(Rml::CompiledGeometryHandle handle,Rml::Vector2f,Rml::TextureHandle) override {
    if(const auto found=matched.find(handle);found!=matched.end() && found->second>=0)++drawn[found->second];
  }
  void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override {matched.erase(handle);}
  Rml::TextureHandle LoadTexture(Rml::Vector2i& size,const Rml::String&) override {size={1280,720};return 1;}
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i) override {return 1;}
  void ReleaseTexture(Rml::TextureHandle) override {}
  void EnableScissorRegion(bool) override {}
  void SetScissorRegion(Rml::Rectanglei) override {}
  void SetTransform(const Rml::Matrix4f*) override {}
  Rml::LayerHandle PushLayer() override {return 1;}
  void PopLayer() override {}
  void CompositeLayers(Rml::LayerHandle,Rml::LayerHandle,Rml::BlendMode,Rml::Span<const Rml::CompiledFilterHandle>) override {}
  Rml::CompiledFilterHandle CompileFilter(const Rml::String&,const Rml::Dictionary&) override {return 1;}
  void ReleaseFilter(Rml::CompiledFilterHandle) override {}
};
std::string read(const char* path) {
  std::ifstream stream(path,std::ios::binary);check(bool(stream),"input file unavailable");
  return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void run(Rml::Context& context,Clock& clock,Sink& sink,const char* document,const char* style,const char* profile_path) {
  Profile profile;
  if(profile_path)check(!glz::read_json(profile,read(profile_path)),"source animation profile invalid");
  const bool source_meshes=!profile.meshes.empty();
  check(!source_meshes || profile.meshes.size()==4,"source visible mesh count differs");
  sink.expected=profile.meshes;sink.drawn.resize(profile.meshes.size());
  auto markup=read(document);const auto css=read(style);
  const bool visibility_gated=css.find("visibility:hidden")!=std::string::npos;
  const auto at=markup.find("</head>");check(at!=std::string::npos,"source head missing");
  markup.insert(at,"<style>"+css+"</style>");
  std::string encoded;check(!glz::write_json(markup,encoded),"markup JSON failed");
  const auto declaration=profile.declaration.empty()?"{\"version\":2,\"model\":\"declared_document\",\"screen_id\":\"fixture.loading\","
      "\"modal\":true,\"logical_width\":1280,\"logical_height\":720,\"markup\":"+encoded+
      ",\"fields\":[{\"id\":\"status\",\"element\":\"field_0\"}]}":profile.declaration;
  octaryn::client::ui::DeclaredDocumentUi ui;
  check(ui.present(context,declaration,R"({"status":""})"),"source declaration rejected");context.Update();
  check(clock.diagnostics==0,"production Rml rejected markup/style properties");
  auto* doc=context.GetDocument(0);check(doc!=nullptr,"source document missing");
  std::array<Rml::Element*,8> slides{};
  for(unsigned index=0;index<slides.size();++index) {
    slides[index]=doc->GetElementById("loading_slide_"+std::to_string(index));
    check(slides[index]!=nullptr,"source slide missing");
  }
  auto* wheel=source_meshes?nullptr:doc->GetElementById("loading_wheel");check(source_meshes || wheel!=nullptr,"source wheel missing");
  std::vector<Rml::Element*> parts;
  for(const auto& mesh:profile.meshes) {
    auto* element=doc->GetElementById(mesh.element);check(element!=nullptr,"source mesh wrapper missing");
    check(element->GetTagName()=="div" && element->GetNumChildren()==1,"production mesh element was not appended");parts.push_back(element);
  }
  std::array<float,8> previous{};unsigned blended{},wheel_changes{};std::string previous_wheel;
  // Two complete authored cycles, sampled with the production update API.
  // Repeated status refreshes must retain the document and animation timeline.
  const unsigned frames=unsigned(std::ceil((profile.initial_delay_seconds+profile.slide_seconds*8*2)*60));
  for(unsigned frame=0;frame<=frames;++frame) {
    clock.now=double(frame)/60.;
    if(frame%6==0)check(ui.present(context,declaration,R"({"status":""})"),"unchanged field refresh rejected");
    context.Update();check(context.GetDocument(0)==doc,"field refresh replaced document");
    float sum{};unsigned fractional{},visible{};
    for(unsigned index=0;index<slides.size();++index) {
      const float opacity=slides[index]->GetComputedValues().opacity();
      const double cycle=profile.slide_seconds*slides.size();
      const double local=std::fmod(clock.now-index*profile.slide_seconds+cycle,cycle);
      double expected_opacity=local<profile.slide_seconds-profile.fade_seconds?1:
          local<profile.slide_seconds?(profile.slide_seconds-local)/profile.fade_seconds:
          local>=cycle-profile.fade_seconds?(local-cycle+profile.fade_seconds)/profile.fade_seconds:0;
      if(profile.fade_starts_at_event) {
        expected_opacity=index==0?1:0;
        if(clock.now>=profile.initial_delay_seconds) {
          const auto event=unsigned(std::floor((clock.now-profile.initial_delay_seconds)/profile.slide_seconds));
          const unsigned outgoing=event%slides.size(),incoming=(event+1)%slides.size();
          const double elapsed=clock.now-profile.initial_delay_seconds-event*profile.slide_seconds;
          const double fraction=std::min(1.,elapsed/profile.fade_seconds);
          expected_opacity=index==incoming?fraction:index==outgoing?1-fraction:0;
        }
      }
      // Rml's float animation accumulator drifts by a few milliseconds across
      // two 80-second cycles; this bound still catches a changed fade duration.
      if(std::abs(opacity-expected_opacity)>=.004)std::fprintf(stderr,"source_opacity frame=%u time=%.9f slide=%u actual=%.9f expected=%.9f local=%.9f\n",frame,clock.now,index,opacity,expected_opacity,local);
      check(std::abs(opacity-expected_opacity)<.004,"authored slide duration/crossfade differs from source timing");
      if(visibility_gated) {
        const bool draws=slides[index]->GetComputedValues().visibility()==Rml::Style::Visibility::Visible;
        if(draws)++visible;
        check(opacity<=0 || draws,"positive-opacity slide hidden during fade");
        if(opacity==0 && frame%unsigned(profile.slide_seconds*60)==60)check(!draws,"inactive held slide remained visible");
      }
      check(std::isfinite(opacity) && opacity>=0 && opacity<=1,"invalid animated opacity");
      if(frame)check(std::abs(opacity-previous[index])<float(1./(60*profile.fade_seconds)+.005),"slide fade jumped between 60 Hz samples");
      if(opacity>.01f && opacity<.99f)++fractional;
      previous[index]=opacity;sum+=opacity;
    }
    check(std::abs(sum-1)<.002f,"slide sequence has a gap or overlaps outside its fade");
    if(visibility_gated)check(visible>=1 && visible<=2,"visibility gating draws more than the active crossfade pair");
    if(fractional==2)++blended;
    if(source_meshes) {
      for(unsigned index=0;index<parts.size();++index) {
        const auto& mesh=profile.meshes[index];const auto* property=parts[index]->GetProperty("transform");
        if(mesh.seconds>0) {
          check(property!=nullptr,"original controller transform missing");
          const auto transform=property->Get<Rml::TransformPtr>();
          const double phase=std::fmod(clock.now,mesh.seconds)/mesh.seconds;
          const double expected=mesh.endpoint_degrees*3.141592653589793/180*phase;
          if(!transform || transform->GetNumPrimitives()==0) {
            check(std::abs(expected)<.0001,"source rotation became identity away from its initial key");continue;
          }
          check(transform->GetNumPrimitives()==1,"source rotation transformed into incompatible primitive");
          const auto& primitive=transform->GetPrimitive(0);
          check(primitive.type==Rml::TransformPrimitive::ROTATE2D,"source controller is not rotate2d");
          double error=std::abs(primitive.rotate_2d.values[0]-expected);
          if(phase<.0001 || phase>.9999)error=std::min(error,std::abs(error-std::abs(mesh.endpoint_degrees*3.141592653589793/180)));
          check(error<.002,"production Rml source controller differs from source Hermite timing/direction/turns");
        } else check(property==nullptr || property->ToString()=="none","static source bars gained an animation");
      }
      if(frame%6==0) {
        std::fill(sink.drawn.begin(),sink.drawn.end(),0);context.Render();
        for(const auto count:sink.drawn)check(count==1,"production source mesh failed to draw source vertices/UV/indices exactly once");
      }
    } else {
      const auto* transform=wheel->GetProperty("transform");check(transform!=nullptr,"wheel animation missing");
      const auto value=transform->ToString();if(!previous_wheel.empty() && value!=previous_wheel)++wheel_changes;
      previous_wheel=value;
    }
    const double event_elapsed=clock.now<profile.initial_delay_seconds?0:
        std::fmod(clock.now-profile.initial_delay_seconds,profile.slide_seconds);
    const bool hold=profile.fade_starts_at_event?clock.now<profile.initial_delay_seconds ||
        (event_elapsed>profile.fade_seconds+.5 && event_elapsed<profile.fade_seconds+.6):
        frame%unsigned(profile.slide_seconds*60)==60;
    if(hold) {
      const unsigned expected=profile.fade_starts_at_event?clock.now<profile.initial_delay_seconds?0:
          (unsigned((clock.now-profile.initial_delay_seconds)/profile.slide_seconds)+1)%8:
          (frame/unsigned(profile.slide_seconds*60))%8;
      check(previous[expected]>.999f,"authored slide order or hold differs");
    }
  }
  check(blended>600,"crossfades did not run on both cycles");
  if(!source_meshes)check(wheel_changes>frames-100,"wheel failed continuous rotation");
  check(ui.hide("fixture.loading"),"hide failed");context.Update();
  check(ui.present(context,declaration,R"({"status":""})"),"second loading request rejected");context.Update();
  check(context.GetDocument(0)->GetElementById("loading_slide_0")->GetComputedValues().opacity()>.999f,
      "new loading request failed to restart first slide");
  check(clock.diagnostics==0,"production Rml rendering raised warnings/errors");
  std::printf("loading_animation checks=%u passed=1 samples=%u seconds=%.2f slides=8 blended_samples=%u wheel_changes=%u visibility_gated=%u source_meshes=%zu production_rml=1 gpu=0 os_input=0\n",
      checks,frames+1,frames/60.,blended,wheel_changes,unsigned(visibility_gated),profile.meshes.size());
}
}
int main(int argc,char** argv) {
  if(argc!=3 && argc!=4)return 2;
  Clock clock;Sink sink;Rml::SetSystemInterface(&clock);Rml::SetRenderInterface(&sink);
  if(!Rml::Initialise())return 2;
  int result{};
  try {auto* context=Rml::CreateContext("loading",{960,540});check(context!=nullptr,"context missing");
    run(*context,clock,sink,argv[1],argv[2],argc==4?argv[3]:nullptr);Rml::RemoveContext("loading");
  } catch(const std::exception& error) {std::fprintf(stderr,"loading_animation checks=%u failed=%s\n",checks,error.what());result=1;}
  Rml::Shutdown();Rml::SetSystemInterface(nullptr);Rml::SetRenderInterface(nullptr);return result;
}
