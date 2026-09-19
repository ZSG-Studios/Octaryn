// The binder include is extracted verbatim from production by the Python runner.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "CatalogCount.h"

#define SLANG_SUCCEEDED(value) ((value) >= 0)
#define SLANG_FAILED(value) ((value) < 0)

namespace rhi {
struct Binding {
  int resource;
  explicit Binding(int value):resource(value) {}
};
struct Field {
  bool valid=true;
  int data_result=0,binding_result=0;
  unsigned data_calls=0,binding_calls=0;
  std::size_t bytes=0;
  std::uint32_t count=0;
  int resource=0;
};
struct IShaderObject {
  std::unordered_map<std::string,Field> fields;
};
struct ShaderCursor {
  IShaderObject* root=nullptr;
  Field* field=nullptr;
  explicit ShaderCursor(IShaderObject* value):root(value) {}
  explicit ShaderCursor(Field* value):field(value) {}
  ShaderCursor operator[](const char* name) const {
    const auto found=root->fields.find(name);
    return ShaderCursor(found==root->fields.end()?nullptr:&found->second);
  }
  bool isValid() const { return field && field->valid; }
  int setData(const void* value,std::size_t size) {
    if(!isValid())return -1;
    ++field->data_calls;field->bytes=size;
    if(field->data_result<0)return field->data_result;
    if(size!=sizeof(std::uint32_t))return -1;
    std::memcpy(&field->count,value,size);
    return field->data_result;
  }
  int setBinding(Binding value) {
    if(!isValid())return -1;
    ++field->binding_calls;field->resource=value.resource;
    return field->binding_result;
  }
};
}

namespace octaryn::client::rendering {
struct WorldAtlas {
  std::vector<std::uint32_t> material_flags;
  int materials=101;
  std::array<int,3> views{102,103,104};
  int cutout=105,linear=106,sprite=107;
};
#include "WorldAtlasBinder.h"
}

namespace {
unsigned failures=0;
void check(bool value,const char* message) {
  if(!value) { ++failures;std::fprintf(stderr,"FAIL: %s\n",message); }
}
constexpr const char* count_field="voxelTraceMaterialCount";
constexpr std::array<const char*,7> resources{
  "blockMaterials","atlasAlbedo","atlasNormal","atlasSpecular","atlasCutout","atlasLinear","atlasSprite"};
rhi::IShaderObject shader(bool count=true) {
  rhi::IShaderObject root;
  for(auto name:resources)root.fields.emplace(name,rhi::Field{});
  if(count)root.fields.emplace(count_field,rhi::Field{});
  return root;
}
void check_resources(const rhi::IShaderObject& root) {
  for(unsigned i=0;i<resources.size();++i) {
    const auto& field=root.fields.at(resources[i]);
    check(field.binding_calls==1 && field.resource==101+static_cast<int>(i),
          "existing atlas resource was not bound exactly once to the correct resource");
  }
}
}

int main() {
  using namespace octaryn::client::rendering;
  WorldAtlas atlas;
  atlas.material_flags.resize(catalog_count);
  auto root=shader();
  check(bind_world_atlas(&atlas,&root),"catalog bind failed");
  const auto& count=root.fields.at(count_field);
  check(count.count==catalog_count,"catalog count including air was not uploaded");
  check(count.data_calls==1 && count.bytes==sizeof(std::uint32_t),"count must be one uint32 upload");
  check_resources(root);

  // A live size change catches a hardcoded catalog count or a stale count.
  atlas.material_flags.resize(catalog_count+3);
  root=shader();
  check(bind_world_atlas(&atlas,&root),"resized catalog bind failed");
  check(root.fields.at(count_field).count==catalog_count+3,"binder ignored current atlas size");
  atlas.material_flags.resize(catalog_count);

  root=shader(false);
  check(bind_world_atlas(&atlas,&root),"optimized-away count must be optional");
  check_resources(root);
  root=shader();
  root.fields.at(count_field).valid=false;
  root.fields.at(count_field).data_result=-1;
  check(bind_world_atlas(&atlas,&root),"invalid count cursor must be optional");
  check(root.fields.at(count_field).data_calls==0,"invalid count cursor was written");
  check_resources(root);

  root=shader();
  root.fields.at(count_field).data_result=-1;
  check(!bind_world_atlas(&atlas,&root),"setData failure must propagate");
  check(root.fields.at(count_field).data_calls==1,"setData failure was not exercised");
  for(auto name:resources)check(root.fields.at(name).binding_calls==0,"failed count upload must stop binding");

  for(auto name:resources) {
    root=shader();
    root.fields.at(name).binding_result=-1;
    check(!bind_world_atlas(&atlas,&root),"resource binding failure must propagate");
  }
  root=shader();
  check(!bind_world_atlas(nullptr,&root),"null atlas must fail");
  check(root.fields.at(count_field).data_calls==0,"null atlas wrote count");
  check(!bind_world_atlas(&atlas,nullptr),"null root must fail");
  check(!bind_world_atlas(nullptr,nullptr),"null atlas and root must fail");
  std::printf("world_atlas_binding catalog_count=%u includes_air=1 failures=%u\n",catalog_count,failures);
  return failures?1:0;
}
