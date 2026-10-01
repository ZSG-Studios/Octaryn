#include "GltfBufferViews.h"
#include <stdexcept>

namespace octaryn::assets {
namespace {
void require(bool valid,const char* reason) {if(!valid)throw std::runtime_error(reason);}
void range(const fastgltf::Asset& asset,std::size_t view_index,std::size_t offset,
    std::size_t count,std::size_t bytes,bool stride) {
  require(view_index<asset.bufferViews.size(),"accessor buffer view index out of range");
  const auto& view=asset.bufferViews[view_index];
  const auto step=stride?view.byteStride.value_or(bytes):bytes;
  require(bytes>0 && step>=bytes && count>0 && offset<=view.byteLength && bytes<=view.byteLength-offset,
      "accessor byte range exceeds buffer view");
  require(count-1<=(view.byteLength-offset-bytes)/step,"accessor element count exceeds buffer view");
}
}
void validate_gltf_accessors(const fastgltf::Asset& asset) {
  for(const auto& accessor:asset.accessors) {
    require(accessor.type!=fastgltf::AccessorType::Invalid && accessor.componentType!=fastgltf::ComponentType::Invalid,
        "invalid accessor element format");
    const auto bytes=fastgltf::getElementByteSize(accessor.type,accessor.componentType);
    if(accessor.bufferViewIndex)range(asset,*accessor.bufferViewIndex,accessor.byteOffset,accessor.count,bytes,true);
    if(accessor.sparse) {
      const auto& sparse=*accessor.sparse;
      require(sparse.count<=accessor.count,"sparse accessor exceeds element count");
      require(sparse.indexComponentType==fastgltf::ComponentType::UnsignedByte ||
          sparse.indexComponentType==fastgltf::ComponentType::UnsignedShort ||
          sparse.indexComponentType==fastgltf::ComponentType::UnsignedInt,"invalid sparse index format");
      range(asset,sparse.indicesBufferView,sparse.indicesByteOffset,sparse.count,
          fastgltf::getComponentByteSize(sparse.indexComponentType),false);
      range(asset,sparse.valuesBufferView,sparse.valuesByteOffset,sparse.count,bytes,false);
    }
  }
}
}
