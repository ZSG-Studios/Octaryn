#include "ScenePreparation.h"
#include "GltfBufferViews.h"
#include "ResourceDigest.h"
#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <glaze/glaze.hpp>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
namespace octaryn::scene_loading {
struct BufferUri {std::string uri;std::uint64_t byteLength{};};
struct ImageUri {std::string uri;std::optional<std::uint32_t> bufferView;};
struct ExternalUris {std::vector<BufferUri> buffers;std::vector<ImageUri> images;};
namespace {
std::filesystem::path local_uri(const Preparation& work,const std::filesystem::path& parent,const std::string& value) {
    fastgltf::URI uri(value);require(uri.isLocalPath(),"scene URI must name a local external file");
    return work.relative(parent,uri.fspath().generic_string());
}
using namespace fastgltf;
using namespace fastgltf::math;
fmat4x4 matrix(const Node& node) {
    if(const auto* trs=std::get_if<TRS>(&node.transform))
        return scale(rotate(translate(fmat4x4(),trs->translation),trs->rotation),trs->scale);
    return std::get<fmat4x4>(node.transform);
}
void walk(const Preparation& work,const Asset& asset,std::size_t index,const fmat4x4& parent,
          std::vector<bool>& visited,std::size_t depth,const std::vector<std::uint64_t>& triangles,Snapshot& result) {
    work.check();require(depth<=1024 && index<asset.nodes.size() && !visited[index],"invalid, repeated or cyclic scene node");
    visited[index]=true;const auto& node=asset.nodes[index];const auto transform=parent*matrix(node);
    require(!node.skinIndex,"static scene metadata cannot discard skinning");
    if(node.meshIndex) {
        require(*node.meshIndex<asset.meshes.size(),"scene mesh index exceeds metadata");
        Instance instance;instance.node=static_cast<std::uint32_t>(index);instance.mesh=static_cast<std::uint32_t>(*node.meshIndex);
        instance.name=node.name;
        for(std::size_t column=0;column<4;++column)for(std::size_t row=0;row<4;++row) {
            require(std::isfinite(transform[column][row]),"scene instance transform is nonfinite");
            instance.transform[column*4+row]=transform[column][row];
        }
        const auto& m=instance.transform;
        const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
            double(m[4])*(double(m[1])*m[10]-double(m[2])*m[9])+double(m[8])*(double(m[1])*m[6]-double(m[2])*m[5]);
        require(std::isfinite(determinant) && std::abs(determinant)>1e-20 && m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1,
                "scene instance requires an invertible affine transform");
        add(result.instanced_triangles,triangles[instance.mesh]);result.instances.push_back(std::move(instance));
    }
    for(const auto child:node.children)walk(work,asset,child,transform,visited,depth+1,triangles,result);
}
}
void gltf_metadata(const Preparation& work,const std::filesystem::path& source,Snapshot& result) {
    work.check();std::set<std::filesystem::path> resources{source};const auto text=work.text(source);
    const auto sourceHash=content::resource_digest({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()});
    {
        ExternalUris uris;constexpr glz::opts options{.error_on_unknown_keys=false};
        require(!glz::read<options>(uris,text),"invalid glTF external resource declarations");
        require(uris.buffers.size()<=4096 && uris.images.size()<=4096,"glTF external resource count limit exceeded");
        for(const auto& buffer:uris.buffers) {
            const auto path=local_uri(work,source.parent_path(),buffer.uri);
            require(buffer.byteLength<=std::filesystem::file_size(path),"glTF external buffer is truncated");resources.insert(path);
        }
        for(const auto& image:uris.images) {
            if(!image.uri.empty())resources.insert(local_uri(work,source.parent_path(),image.uri));
            else require(image.bufferView.has_value(),"glTF image has no source");
        }
    }
    auto mapped=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(text.data()),text.size());
    require(mapped.error()==fastgltf::Error::None,"glTF metadata map failed");
    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit |
                           fastgltf::Extensions::EXT_meshopt_compression);
    auto loaded=parser.loadGltf(mapped.get(),source.parent_path(),fastgltf::Options::None);
    require(loaded.error()==fastgltf::Error::None,"glTF metadata parser failed");const auto& asset=loaded.get();
    require(fastgltf::validate(asset)==fastgltf::Error::None,"invalid glTF scene metadata");
    assets::validate_gltf_accessors(asset);
    require(asset.nodes.size()<=65536 && asset.meshes.size()<=65536 && asset.accessors.size()<=65536,
            "glTF metadata exceeds object limit");
    require(!asset.meshes.empty() && !asset.scenes.empty(),"glTF contains no scene meshes");
    require(asset.animations.empty(),"static prepared scene cannot silently discard animations");
    result.source=source;result.meshes=static_cast<std::uint32_t>(asset.meshes.size());
    result.materials=static_cast<std::uint32_t>(asset.materials.size());
    std::vector<std::uint64_t> triangles(asset.meshes.size());
    for(std::size_t mesh=0;mesh<asset.meshes.size();++mesh)for(std::size_t id=0;id<asset.meshes[mesh].primitives.size();++id) {
        work.check();const auto& primitive=asset.meshes[mesh].primitives[id];
        require(primitive.targets.empty(),"static prepared scene cannot discard morph targets");
        const auto position=primitive.findAttribute("POSITION");
        require(position!=primitive.attributes.end() && position->accessorIndex<asset.accessors.size(),"scene primitive positions missing");
        const auto& vertices=asset.accessors[position->accessorIndex];
        require(vertices.type==fastgltf::AccessorType::Vec3 && vertices.count,"scene POSITION accessor invalid");
        require(!primitive.indicesAccessor || *primitive.indicesAccessor<asset.accessors.size(),"scene indices accessor invalid");
        const auto count=primitive.indicesAccessor?asset.accessors[*primitive.indicesAccessor].count:vertices.count;
        require(count>=3,"scene primitive has no triangles");std::uint64_t number{};
        if(primitive.type==fastgltf::PrimitiveType::Triangles) {require(count%3==0,"partial scene triangle");number=count/3;}
        else if(primitive.type==fastgltf::PrimitiveType::TriangleStrip || primitive.type==fastgltf::PrimitiveType::TriangleFan)number=count-2;
        else throw std::runtime_error("prepared scene requires triangle primitives");
        require(result.primitives.size()<65536,"scene primitive metadata count exceeded");
        result.primitives.push_back({static_cast<std::uint32_t>(mesh),static_cast<std::uint32_t>(id),
            primitive.materialIndex?static_cast<std::uint32_t>(*primitive.materialIndex):UINT32_MAX,vertices.count,number});
        add(result.unique_triangles,number);add(triangles[mesh],number);
    }
    const auto selected=asset.defaultScene.value_or(0);
    require(selected<asset.scenes.size(),"default glTF scene is invalid");std::vector<bool> visited(asset.nodes.size());
    for(const auto node:asset.scenes[selected].nodeIndices)walk(work,asset,node,fastgltf::math::fmat4x4(),visited,0,triangles,result);
    require(!result.instances.empty(),"prepared scene contains no active mesh instances");
    for(const auto& path:resources)result.resources.push_back({path,path==source?sourceHash:std::string{},
        path==source?text.size():std::filesystem::file_size(path),false});
}
}
