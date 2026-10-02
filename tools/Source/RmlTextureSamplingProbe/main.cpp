#include "../../../octaryn-client/Source/Rendering/Ui/RmlTextureSource.h"
#include <cstdio>
#include <string>

int main() {
  using namespace octaryn::client::rendering;
  struct Case {const char* prefix;bool linear,wrap,straight;};
  constexpr Case cases[]={{"",false,false,false},{"octaryn-ui-wrap:",false,true,false},
    {"octaryn-ui-linear:",true,false,false},{"octaryn-ui-linear-wrap:",true,true,false},
    {"octaryn-ui-straight:",false,false,true},{"octaryn-ui-straight-wrap:",false,true,true},
    {"octaryn-ui-straight-linear:",true,false,true},{"octaryn-ui-straight-linear-wrap:",true,true,true}};
  int checks=0;
  for(const auto& c:cases) {
    const std::string source=std::string(c.prefix)+"C:/module/Assets/atlas.png";
    const auto p=rml_texture_source(source);
    if(p.filename!="C:/module/Assets/atlas.png" || p.linear!=c.linear || p.wrap!=c.wrap || p.straight!=c.straight)return 1;
    ++checks;
    const auto wrapped=rml_wrapped_texture_source(source);const auto w=rml_texture_source(wrapped);
    if(w.filename!=p.filename || w.linear!=p.linear || !w.wrap || w.straight!=p.straight)return 2;
    ++checks;
    if(rml_wrapped_texture_source(wrapped)!=wrapped)return 3;
    ++checks;
  }
  std::printf("rml_texture_sampling_checks=%d passed=1 point_default=1 wrap_idempotent=1 alpha_policy=1\n",checks);
}
