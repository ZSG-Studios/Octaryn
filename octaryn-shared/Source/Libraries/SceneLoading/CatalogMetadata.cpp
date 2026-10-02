#include "ScenePreparation.h"
#include "ResourceDigest.h"
#include <glaze/glaze.hpp>
#include <cmath>
#include <map>
#include <set>
namespace octaryn::scene_loading {
struct CatalogResource {std::string path,hash;std::uint64_t bytes{};};
struct CatalogPrimitive {
    std::uint32_t mesh{},primitive{},material{UINT32_MAX},first_part{},part_count{};
    std::uint64_t triangles{},vertices{};bool position_only{};
    std::string triangle_order,triangle_order_hash;
};
struct CatalogPart {
    std::uint32_t primitive{},clusters{},pages{},root_pages{};
    std::uint64_t first_triangle{},triangle_count{};std::string geometry,hash;
};
struct Catalog {
    std::uint32_t version{},part_triangles{},mesh_count{},material_count{};
    std::uint64_t unique_triangles{},instanced_triangles{};std::string source,source_hash;
    std::vector<CatalogResource> resources;std::vector<CatalogPrimitive> primitives;
    std::vector<CatalogPart> parts;std::vector<Instance> instances;
};
namespace {
std::filesystem::path named(const Preparation& work,const std::filesystem::path& parent,const std::string& value) {
    const auto path=std::filesystem::path(reinterpret_cast<const char8_t*>(value.c_str()));
    return path.is_absolute()?work.confined(path):work.relative(parent,value);
}
std::string digest(const std::string& value) {
    return content::resource_digest({reinterpret_cast<const std::uint8_t*>(value.data()),value.size()});
}
}
void catalog_metadata(const Preparation& work,const std::filesystem::path& path,Snapshot& output) {
    Catalog catalog;constexpr glz::opts options{.error_on_unknown_keys=false};std::string catalogHash;std::uint64_t catalogBytes{};
    {const auto text=work.text(path);require(!glz::read<options>(catalog,text),"invalid scene catalog JSON");catalogHash=digest(text);catalogBytes=text.size();}
    require(catalog.version==4 && catalog.part_triangles && catalog.part_triangles<=65536 && catalog.mesh_count &&
            catalog.mesh_count<=65536 && !catalog.primitives.empty() && !catalog.instances.empty() &&
            catalog.parts.size()<=65536 && catalog.instances.size()<=65536 && catalog.primitives.size()<=65536 &&
            !catalog.resources.empty() && catalog.resources.size()<=4096 && hash_valid(catalog.source_hash),"scene catalog limits or identity invalid");
    output.source=named(work,path.parent_path(),catalog.source);output.catalog=path;
    output.meshes=catalog.mesh_count;output.materials=catalog.material_count;output.identity=catalog.source_hash;
    std::string identity;std::set<std::filesystem::path> resources;
    for(const auto& resource:catalog.resources) {
        const auto file=named(work,path.parent_path(),resource.path);
        require(hash_valid(resource.hash) && resources.insert(file).second,"catalog source resource duplicated or invalid");
        output.resources.push_back({file,resource.hash,resource.bytes,true});identity+=resource.hash;
    }
    require(resources.contains(output.source) && digest(identity)==catalog.source_hash,"catalog source identity differs from resource digests");
    std::vector<std::uint64_t> meshes(catalog.mesh_count);std::uint64_t total{};std::size_t next{};
    std::set<std::pair<std::uint32_t,std::uint32_t>> primitives;
    std::map<std::filesystem::path,std::string> orders;
    for(std::uint32_t id=0;id<catalog.primitives.size();++id) {
        work.check();const auto& primitive=catalog.primitives[id];
        require(primitive.mesh<catalog.mesh_count && primitive.vertices && primitive.triangles &&
                (primitive.material==UINT32_MAX || primitive.material<catalog.material_count) &&
                primitives.emplace(primitive.mesh,primitive.primitive).second && primitive.first_part==next &&
                primitive.part_count && primitive.part_count<=catalog.parts.size()-next,"catalog primitive coverage invalid");
        if(!primitive.triangle_order.empty()) {
            require(hash_valid(primitive.triangle_order_hash),"catalog triangle-order identity invalid");
            const auto order=work.relative(path.parent_path(),primitive.triangle_order);
            // Primitives may share one order file; its identity must agree.
            const auto [known,added]=orders.emplace(order,primitive.triangle_order_hash);
            require(known->second==primitive.triangle_order_hash,"catalog triangle-order identity conflicts");
            if(added)output.resources.push_back({order,primitive.triangle_order_hash,std::filesystem::file_size(order),true});
        }else require(primitive.triangle_order_hash.empty(),"catalog source-order primitive has stray digest");
        std::uint64_t first{};
        for(std::uint32_t index=0;index<primitive.part_count;++index) {
            const auto& part=catalog.parts[next++];
            require(part.primitive==id && part.first_triangle==first && part.triangle_count && part.triangle_count<=catalog.part_triangles,
                    "catalog triangle coverage has a gap or overlap");add(first,part.triangle_count);
            if(!part.geometry.empty()) {
                const auto cooked=work.relative(path.parent_path(),part.geometry);
                require(hash_valid(part.hash) && part.pages && part.clusters && part.root_pages && part.root_pages<=part.pages,
                        "catalog cooked part metadata invalid");
                const auto key=catalog.source_hash+":2:"+std::to_string(primitive.mesh)+":"+std::to_string(primitive.primitive)+":"+
                    std::to_string(part.first_triangle)+":"+std::to_string(part.triangle_count)+
                    (primitive.position_only?":flat-position":":full-attributes")+
                    (primitive.triangle_order.empty()?"":":order="+primitive.triangle_order_hash);
                require(digest(key)==part.hash,"cooked part identity does not match source triangle window");
                cook_identity(work,cooked,part.hash,part.triangle_count,part.clusters,part.pages);
            }else require(part.hash.empty() && !part.pages && !part.clusters && !part.root_pages,"uncooked part has cooked metadata");
        }
        require(first==primitive.triangles,"catalog primitive has incomplete coverage");
        add(total,first);add(meshes[primitive.mesh],first);
        output.primitives.push_back({primitive.mesh,primitive.primitive,primitive.material,primitive.vertices,primitive.triangles});
    }
    require(next==catalog.parts.size() && total==catalog.unique_triangles,"catalog unique source coverage incomplete");
    std::set<std::uint32_t> nodes;std::uint64_t instanced{};
    for(const auto& instance:catalog.instances) {
        require(instance.mesh<meshes.size() && nodes.insert(instance.node).second,"catalog instance identity invalid");
        for(float value:instance.transform)require(std::isfinite(value),"catalog transform is nonfinite");
        const auto& m=instance.transform;
        const double det=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
            double(m[4])*(double(m[1])*m[10]-double(m[2])*m[9])+double(m[8])*(double(m[1])*m[6]-double(m[2])*m[5]);
        require(std::isfinite(det) && std::abs(det)>1e-20 && m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1,"catalog affine transform invalid");
        add(instanced,meshes[instance.mesh]);
    }
    require(instanced==catalog.instanced_triangles,"catalog instanced source coverage incomplete");
    output.unique_triangles=total;output.instanced_triangles=instanced;output.instances=std::move(catalog.instances);
    output.resources.push_back({path,catalogHash,catalogBytes,false});
}
}
