#include "DebugMetrics.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unordered_set>

using octaryn::client::app::DebugMetrics;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct System final:Rml::SystemInterface {
  unsigned failures{};
  bool LogMessage(Rml::Log::Type type,const Rml::String& text)override {
    if(type==Rml::Log::LT_WARNING || type==Rml::Log::LT_ERROR || type==Rml::Log::LT_ASSERT) {
      ++failures;std::cerr<<text<<'\n';
    }return true;
  }
};
struct Renderer final:Rml::RenderInterface {
  size_t compiled{},released{},peak{};
  std::unordered_set<Rml::CompiledGeometryHandle> live;
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>)override {
    const auto handle=++compiled;live.insert(handle);peak=std::max(peak,live.size());return handle;
  }
  void ReleaseGeometry(Rml::CompiledGeometryHandle handle)override {require(live.erase(handle)==1,"invalid geometry lifetime");++released;}
  void RenderGeometry(Rml::CompiledGeometryHandle handle,Rml::Vector2f,Rml::TextureHandle)override {require(live.contains(handle),"render released geometry");}
  Rml::TextureHandle LoadTexture(Rml::Vector2i& size,const Rml::String&)override {size={32,32};return 1;}
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i)override{return 1;}
  void ReleaseTexture(Rml::TextureHandle)override{}
  void EnableScissorRegion(bool)override{}
  void SetScissorRegion(Rml::Rectanglei)override{}
};
struct Result {double milliseconds;size_t compiled,peak;};
Result run(Renderer& renderer,const std::string& assets,bool retained) {
  auto* context=Rml::CreateContext(retained?"retained":"replacement",{1280,720});require(context,"create context");
  auto* document=context->LoadDocument(assets+"/game.rml");require(document,"load real UI document");
  for(const char* id:{"menu","lighting","scrim"})document->GetElementById(id)->SetClass("hidden",true);
  document->GetElementById("diagnostics")->SetClass("hidden",false);document->Show();
  auto* metrics=document->GetElementById("metrics");require(metrics,"real metrics owner");
  DebugMetrics owner;DebugMetrics::Values values{};values.fill(1234);values[14]=UINT32_MAX;
  if(retained)owner.update(metrics,values);else metrics->SetInnerRML(DebugMetrics::markup(values));
  context->Update();context->Render();
  auto* row=metrics->GetChild(0);auto* label=row->GetChild(0);auto* stable_row=metrics->GetChild(14);
  const auto live_before=renderer.live.size(),compiled_before=renderer.compiled;
  const auto started=std::chrono::steady_clock::now();
  for(unsigned i=0;i<400;++i) {
    values[0]=1000+i%100;values[1]=1200+i%100;values[17]=i%100;values[18]=2000+i%100;
    if(retained)owner.update(metrics,values);else metrics->SetInnerRML(DebugMetrics::markup(values));
    context->Update();context->Render();
    if(retained)require(metrics->GetChild(0)==row && row->GetChild(0)==label && metrics->GetChild(14)==stable_row,
        "metric refresh must retain rows and labels");
    require(renderer.live.size()<=live_before+4,"geometry retention grows with refresh count");
  }
  const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  const auto count=renderer.compiled-compiled_before;
  const auto unchanged=renderer.compiled;
  for(int i=0;i<40;++i) {
    if(retained)owner.update(metrics,values);
    context->Update();context->Render();
  }
  require(renderer.compiled==unchanged,"unchanged telemetry recompiles geometry");
  require(metrics->GetInnerRML().find("N/A")!=std::string::npos,"unavailable telemetry preserved");
  Rml::RemoveContext(context->GetName());
  require(renderer.live.empty(),"document teardown leaks retained geometry");
  return {elapsed,count,renderer.peak};
}
}
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  System system;Renderer renderer;bool initialized=false;
  try {
    Rml::SetSystemInterface(&system);Rml::SetRenderInterface(&renderer);
    require(Rml::Initialise(),"Rml initialize");initialized=true;
    const auto assets=std::filesystem::path(argv[1]).generic_string();
    require(Rml::LoadFontFace(assets+"/Fonts/Silkscreen-Regular.ttf"),"load original UI font");
    const auto old=run(renderer,assets,false),fixed=run(renderer,assets,true);
    require(fixed.compiled<old.compiled,"retained metrics must reduce actual Rml geometry compilation");
    Rml::Shutdown();initialized=false;require(system.failures==0,"Rml diagnostics");
    std::cout<<"ui_metrics=passed refreshes=400 old_compile="<<old.compiled<<" retained_compile="<<fixed.compiled
      <<" old_cpu_ms="<<old.milliseconds<<" retained_cpu_ms="<<fixed.milliseconds
      <<" peak_geometry="<<fixed.peak<<" teardown_live="<<renderer.live.size()<<" gpu_created=0\n";return 0;
  }catch(const std::exception& e){if(initialized)Rml::Shutdown();std::cerr<<"ui_metrics failed: "<<e.what()<<'\n';return 1;}
}
