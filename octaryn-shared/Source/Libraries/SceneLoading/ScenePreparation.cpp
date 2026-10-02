#include "ScenePreparation.h"
#include "SceneManifest.h"
#include "ResourceDigest.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace octaryn::scene_loading {
struct MetadataKind {std::uint32_t version{};std::string format;};
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool hash_valid(const std::string& value) {
    return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
void add(std::uint64_t& value,std::uint64_t amount) {
    require(amount<=UINT64_MAX-value,"scene count overflow");value+=amount;
}
void Preparation::check() const {require(!cancel || !cancel->load(std::memory_order_relaxed),"scene preparation canceled");}
std::filesystem::path Preparation::confined(const std::filesystem::path& value) const {
    check();const auto path=std::filesystem::canonical(content::file_io_path(value));
    const auto relative=path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute(),"scene resource outside module root");
    for(const auto& component:relative)require(component!="..","scene resource link escapes module root");
    require(std::filesystem::is_regular_file(path),"scene resource is not a regular file");return path;
}
std::filesystem::path Preparation::relative(const std::filesystem::path& parent,const std::string& value) const {
    require(!value.empty() && value.find('\0')==std::string::npos && value.find(':')==std::string::npos,"invalid scene relative resource");
    const auto path=std::filesystem::path(reinterpret_cast<const char8_t*>(value.c_str()));
    require(!path.is_absolute() && !path.has_root_name(),"scene resource requires relative path");
    for(const auto& component:path)require(component!="..","scene resource path escapes package");
    return confined(parent/path);
}
std::string Preparation::text(const std::filesystem::path& path) const {
    check();const auto size=std::filesystem::file_size(path);
    require(size && size<=4ull*1024*1024,"scene metadata exceeds 4 MiB file limit");
    const auto stamp=std::filesystem::last_write_time(path);
    std::ifstream file(path,std::ios::binary);std::string result(static_cast<size_t>(size),'\0');
    require(bool(file.read(result.data(),static_cast<std::streamsize>(size))),"scene metadata read failed");
    require(stamp==std::filesystem::last_write_time(path) && size==std::filesystem::file_size(path),"scene metadata changed while reading");
    std::uint64_t structures{};bool quoted=false,escaped=false;
    for(char c:result) {
        if(quoted) {if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;else if(c=='{' || c=='[')++structures;
    }
    require(size*8+structures*512+1024*1024<=working_bytes,"scene metadata exceeds parser working reservation");
    check();return result;
}
void verify_resources(const Preparation& work,Snapshot& snapshot) {
    std::uint64_t total{},completed{};std::set<std::filesystem::path> paths;
    for(const auto& resource:snapshot.resources) {
        require(paths.insert(resource.path).second,"duplicate scene resource");
        require(resource.bytes<=resource_limit,"scene resource exceeds 16 GiB verification limit");
        add(total,resource.bytes);require(total<=verification_limit,"scene resources exceed 64 GiB total verification limit");
    }
    work.progress(0,total);std::string identity;
    for(auto& resource:snapshot.resources) {
        work.check();require(std::filesystem::file_size(resource.path)==resource.bytes,"scene resource size changed");
        std::string error;const auto digest=resource.tree_hash?
            content::resource_tree_digest(resource.path,error,resource_limit,work.cancel):
            content::resource_file_digest(resource.path,error,resource_limit,work.cancel);
        if(digest.empty())throw std::runtime_error(error);
        require(resource.hash.empty() || resource.hash==digest,"scene resource digest mismatch");resource.hash=digest;
        const auto relative=resource.path.lexically_relative(work.root).generic_u8string();
        identity+=std::string(reinterpret_cast<const char*>(relative.data()),relative.size())+":"+
            std::to_string(resource.bytes)+":"+digest+";";
        add(completed,resource.bytes);work.progress(completed,total);
    }
    if(snapshot.identity.empty())snapshot.identity=content::resource_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
}
namespace {
Manifest manifest(const Preparation& work,const std::filesystem::path& descriptor,std::string& captured,std::uint64_t& bytes) {
    Manifest value;constexpr glz::opts options{.error_on_unknown_keys=false};
    const auto text=work.text(descriptor);
    captured=content::resource_digest({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()});bytes=text.size();
    require(!glz::read<options>(value,text),"invalid scene import descriptor JSON");
    require(value.version==1 && value.prepared && !value.format.empty() && !value.files.empty() && value.files.size()<=4096,
            "scene import descriptor is unprepared or exceeds file count limit");return value;
}
std::uint64_t retained(const Snapshot& value) {
    std::uint64_t bytes=sizeof(Snapshot)+value.identity.capacity();
    add(bytes,value.source.native().size()*sizeof(std::filesystem::path::value_type));
    add(bytes,value.catalog.native().size()*sizeof(std::filesystem::path::value_type));
    add(bytes,value.resources.capacity()*sizeof(Resource));add(bytes,value.primitives.capacity()*sizeof(Primitive));
    add(bytes,value.instances.capacity()*sizeof(Instance));
    for(const auto& resource:value.resources) {
        add(bytes,resource.path.native().size()*sizeof(std::filesystem::path::value_type));add(bytes,resource.hash.capacity());
    }
    for(const auto& instance:value.instances)add(bytes,instance.name.capacity());
    return bytes;
}
}
std::unique_ptr<Snapshot> prepare(const Preparation& work) {
    work.check();const auto input=work.confined(work.input);auto result=std::make_unique<Snapshot>();
    std::filesystem::path descriptor;
    if(input.extension()==".gltf") {
        const auto adjacent=input.parent_path()/"scene-import.json";
        if(std::filesystem::exists(adjacent))descriptor=work.confined(adjacent);
        else gltf_metadata(work,input,*result);
    }else if(input.extension()==".json") {
        MetadataKind kind;
        const auto text=work.text(input);constexpr glz::opts options{.error_on_unknown_keys=false};
        require(!glz::read<options>(kind,text),"invalid scene JSON");
        if(kind.version==4 && kind.format.empty())catalog_metadata(work,input,*result);
        else descriptor=input;
    }else throw std::runtime_error("runtime scene loading requires prepared .gltf or JSON catalog; import USD offline");
    if(!descriptor.empty()) {
        std::string descriptorHash;std::uint64_t descriptorBytes{};
        const auto value=manifest(work,descriptor,descriptorHash,descriptorBytes);const auto source=work.relative(descriptor.parent_path(),value.scene);
        require(input.extension()!=".gltf" || input==source,"adjacent descriptor selects a different scene");
        require(source.extension()==".gltf","import descriptor requires glTF JSON metadata");
        gltf_metadata(work,source,*result);std::map<std::filesystem::path,Resource> declared;
        for(const auto& file:value.files) {
            require(hash_valid(file.sha256),"invalid scene file SHA256");const auto path=work.relative(descriptor.parent_path(),file.path);
            require(declared.emplace(path,Resource{path,file.sha256,file.bytes,false}).second,"duplicate descriptor file");
        }
        for(const auto& resource:result->resources) {
            const auto found=declared.find(resource.path);
            require(found!=declared.end() && found->second.bytes==resource.bytes,"glTF resource absent or size differs from cooked descriptor");
            require(resource.hash.empty() || resource.hash==found->second.hash,"captured glTF metadata differs from cooked descriptor revision");
        }
        require(value.counts.meshes==result->meshes && value.counts.instances==result->instances.size() &&
                value.counts.uniqueTriangles==result->unique_triangles && value.counts.instancedTriangles==result->instanced_triangles,
                "cooked descriptor scene counts differ from source metadata");
        result->resources.clear();for(auto& [path,resource]:declared)result->resources.push_back(std::move(resource));
        result->resources.push_back({descriptor,descriptorHash,descriptorBytes,false});result->catalog=descriptor;
    }
    verify_resources(work,*result);work.check();result->retained_bytes=retained(*result);
    require(result->retained_bytes<=working_bytes/2,"scene snapshot exceeds retained metadata budget");return result;
}
}
