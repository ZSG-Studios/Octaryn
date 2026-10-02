#include "SceneLoading.h"
#include "ResourceDigest.h"
#include "octaryn_native_schedule_runtime.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
namespace {
std::size_t assertions{};
void require(bool value,const std::string& message) {if(!value)throw std::runtime_error(message);++assertions;}
void write(const fs::path& file,const std::string& text) {
    std::ofstream output(file,std::ios::binary);output.write(text.data(),static_cast<std::streamsize>(text.size()));require(bool(output),"fixture write failed");
}
std::string hash(const fs::path& path) {
    std::string error;const auto value=octaryn::content::resource_file_digest(path,error);require(!value.empty(),error);return value;
}
std::string scene() {
    return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1]}],"nodes":[{"mesh":0,"name":"original"},{"mesh":0,"name":"instance","translation":[5,0,0]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"buffers":[{"uri":"scene.bin","byteLength":42}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]})";
}
std::string descriptor(const fs::path& root,const std::string& sourceHash={}) {
    return "{\"version\":1,\"format\":\"nif\",\"scene\":\"scene.gltf\",\"prepared\":true,"
        "\"source\":{\"path\":\"Z:/nonexistent-owned-game/reference.nif\",\"sha256\":\"informational-only\"},"
        "\"files\":[{\"path\":\"scene.gltf\",\"sha256\":\""+(sourceHash.empty()?hash(root/"scene.gltf"):sourceHash)+
        "\",\"bytes\":"+std::to_string(fs::file_size(root/"scene.gltf"))+"},{\"path\":\"scene.bin\",\"sha256\":\""+
        hash(root/"scene.bin")+"\",\"bytes\":42}],\"counts\":{\"meshes\":1,\"uniqueTriangles\":1,\"instances\":2,\"instancedTriangles\":2}}";
}
std::string catalog(const fs::path& root) {
    std::string resources,identity,error;
    for(const auto& name:{"scene.gltf","scene.bin"}) {
        const auto value=octaryn::content::resource_tree_digest(root/name,error);require(!value.empty(),error);identity+=value;
        if(!resources.empty())resources+=',';
        resources+="{\"path\":\""+std::string(name)+"\",\"hash\":\""+value+"\",\"bytes\":"+std::to_string(fs::file_size(root/name))+"}";
    }
    const auto sourceHash=octaryn::content::resource_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
    return "{\"version\":4,\"part_triangles\":65536,\"mesh_count\":1,\"material_count\":0,\"source\":\"scene.gltf\",\"source_hash\":\""+
        sourceHash+"\",\"unique_triangles\":1,\"instanced_triangles\":2,\"resources\":["+resources+"],"
        "\"primitives\":[{\"mesh\":0,\"primitive\":0,\"vertices\":3,\"triangles\":1,\"first_part\":0,\"part_count\":1}],"
        "\"parts\":[{\"primitive\":0,\"first_triangle\":0,\"triangle_count\":1}],"
        "\"instances\":[{\"node\":0,\"mesh\":0,\"name\":\"original\",\"transform\":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]},"
        "{\"node\":1,\"mesh\":0,\"name\":\"instance\",\"transform\":[1,0,0,0,0,1,0,0,0,0,1,0,5,0,0,1]}]}";
}
struct Directory {
    fs::path root=fs::temp_directory_path()/("octaryn-scene-loading-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() {fs::create_directory(root);}
    ~Directory() {std::error_code error;if(root.parent_path()==fs::temp_directory_path())fs::remove_all(root,error);}
};
struct Runtime {
    void* scheduler=octaryn_native_schedule_runtime_create(4,2);void* owner{};
    explicit Runtime(const fs::path& root) {
        require(scheduler!=nullptr,"scheduler creation failed");const auto text=root.generic_u8string();
        owner=octaryn_scene_loading_create("probe.scene",reinterpret_cast<const char*>(text.c_str()),scheduler);
        require(owner!=nullptr,"scene owner creation failed");
    }
    ~Runtime() {octaryn_scene_loading_destroy(owner);octaryn_native_schedule_runtime_destroy(scheduler);}
    octaryn_scene_loading_ticket begin(const fs::path& source) {
        octaryn_scene_loading_ticket ticket{};const auto text=source.generic_u8string();
        require(octaryn_scene_loading_begin(owner,reinterpret_cast<const char*>(text.c_str()),&ticket)==0 && ticket.id && ticket.generation,
                "scene begin failed");return ticket;
    }
    octaryn_scene_loading_progress wait(const octaryn_scene_loading_ticket& ticket) {
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);octaryn_scene_loading_progress result{};
        do {
            if(octaryn_scene_loading_query(owner,&ticket,&result)!=0)throw std::runtime_error("valid scene query failed");
            if(result.publication || result.completed>result.total || result.retained_bytes>16ull*1024*1024)
                throw std::runtime_error("scene progress or budget invalid");
            if(result.preparation>=OCTARYN_SCENE_LOADING_CPU_PREPARED) {assertions+=2;return result;}
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }while(std::chrono::steady_clock::now()<end);
        throw std::runtime_error("scene worker preparation timed out");
    }
    void prepared(const octaryn_scene_loading_ticket& ticket) {
        const auto result=wait(ticket);
        if(result.preparation!=OCTARYN_SCENE_LOADING_CPU_PREPARED) {
            char reason[1025]{};octaryn_scene_loading_error(owner,&ticket,reason,sizeof(reason));throw std::runtime_error(reason);
        }
    }
    void failed(const octaryn_scene_loading_ticket& ticket,const std::string& reason) {
        require(wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"bounded preparation did not fail");
        char text[1025]{};require(octaryn_scene_loading_error(owner,&ticket,text,sizeof(text))==0 &&
            std::string(text).find(reason)!=std::string::npos,"failure did not report expected resource limit");
    }
    void release(const octaryn_scene_loading_ticket& ticket) {
        require(octaryn_scene_loading_release(owner,&ticket)==0,"ticket release failed");octaryn_scene_loading_progress state{};
        require(octaryn_scene_loading_query(owner,&ticket,&state)!=0,"released ticket remained valid");
    }
};
void synthetic() {
    Directory directory;const auto root=directory.root;
    std::string bin(42,0);bin[38]=1;bin[40]=2;write(root/"scene.bin",bin);write(root/"scene.gltf",scene());
    Runtime runtime(root);auto ticket=runtime.begin(root/"scene.gltf");runtime.prepared(ticket);
    auto snapshot=octaryn::scene_loading::snapshot(runtime.owner,ticket);
    require(snapshot && snapshot->meshes==1 && snapshot->unique_triangles==1 && snapshot->instanced_triangles==2 && snapshot->instances.size()==2,
            "native metadata lost shared meshes or instances");
    require(snapshot->instances[1].transform[12]==5 && snapshot->instances[1].name=="instance","original instance matrix/name lost");
    require(snapshot->resources.size()==2 && snapshot->retained_bytes && !snapshot->identity.empty(),"verified immutable snapshot missing");
    runtime.release(ticket);require(snapshot->instances.size()==2,"host snapshot lease invalidated by ticket release");snapshot.reset();
    write(root/"scene-import.json",descriptor(root));ticket=runtime.begin(root/"scene-import.json");runtime.prepared(ticket);
    const auto original=octaryn::scene_loading::snapshot(runtime.owner,ticket)->identity;
    const auto rootText=root.generic_u8string(),descriptorText=(root/"scene-import.json").generic_u8string();
    char verificationError[1024]{};
    require(octaryn_scene_loading_verify(reinterpret_cast<const char*>(rootText.c_str()),
        reinterpret_cast<const char*>(descriptorText.c_str()),verificationError,sizeof(verificationError))==0,
        "replacement reverify failed with retained prepared destination");
    runtime.release(ticket);
    fs::create_directory(root/"copy");
    for(const auto& name:{"scene.gltf","scene.bin","scene-import.json"})fs::copy_file(root/name,root/"copy"/name);
    {Runtime relocated(root/"copy");auto copied=relocated.begin(root/"copy"/"scene-import.json");relocated.prepared(copied);
     require(octaryn::scene_loading::snapshot(relocated.owner,copied)->identity==original,"relocating package changed content identity");relocated.release(copied);}
    write(root/"scene-import.json",descriptor(root,std::string(64,'0')));ticket=runtime.begin(root/"scene-import.json");
    require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"corrupt cooked source digest accepted");runtime.release(ticket);
    write(root/"scene-import.json",descriptor(root));bin[0]=1;write(root/"scene.bin",bin);ticket=runtime.begin(root/"scene.gltf");
    require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"same-size modified cooked buffer accepted");runtime.release(ticket);
    require(octaryn_scene_loading_verify(reinterpret_cast<const char*>(rootText.c_str()),
        reinterpret_cast<const char*>(descriptorText.c_str()),verificationError,sizeof(verificationError))!=0 && verificationError[0],
        "replacement verifier accepted changed buffer");
    bin[0]=0;write(root/"scene.bin",bin);write(root/"scene-import.json",descriptor(root));
    for(const auto& budget:std::array<std::pair<unsigned,std::uint64_t>,2>{{{1,17ull*1024*1024*1024},{5,16ull*1024*1024*1024}}}) {
        auto declared=descriptor(root);std::string extra;
        for(unsigned index=0;index<budget.first;++index) {
            const auto name="budget"+std::to_string(index)+".bin";write(root/name,"x");
            extra+=",{\"path\":\""+name+"\",\"sha256\":\""+std::string(64,'0')+"\",\"bytes\":"+std::to_string(budget.second)+"}";
        }
        declared.insert(declared.find("],\"counts\""),extra);write(root/"scene-import.json",declared);
        ticket=runtime.begin(root/"scene-import.json");runtime.failed(ticket,budget.first==1?"16 GiB":"64 GiB");runtime.release(ticket);
    }
    write(root/"scene-import.json",descriptor(root));
    std::vector<octaryn_scene_loading_ticket> tickets;
    for(unsigned i=0;i<8;++i)tickets.push_back(runtime.begin(root/"scene.gltf"));
    octaryn_scene_loading_ticket denied{};const auto path=(root/"scene.gltf").generic_u8string();
    require(octaryn_scene_loading_begin(runtime.owner,reinterpret_cast<const char*>(path.c_str()),&denied)!=0,"ticket capacity was unbounded");
    for(const auto& current:tickets) {
        require(octaryn_scene_loading_cancel(runtime.owner,&current)==0,"cancel failed");
        require(runtime.wait(current).preparation==OCTARYN_SCENE_LOADING_CANCELED,"cancel allowed late CPU publication");runtime.release(current);
    }
    ticket=runtime.begin(root/"scene.gltf");runtime.prepared(ticket);
    require(std::all_of(tickets.begin(),tickets.end(),[&](const auto& prior){return prior.generation!=ticket.generation;}),"ticket slot reused stale generation");
    require(octaryn_scene_loading_cancel(runtime.owner,&ticket)==0 && runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_CANCELED,
            "cancel did not retire CPU-ready metadata");runtime.release(ticket);
    fs::remove(root/"scene-import.json");auto escaping=scene();const auto at=escaping.find("scene.bin");escaping.replace(at,9,"../outside.bin");
    write(root/"scene.gltf",escaping);ticket=runtime.begin(root/"scene.gltf");
    require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"resource traversal accepted");runtime.release(ticket);
    auto remote=scene();remote.replace(remote.find("scene.bin"),9,"https://example.invalid/scene.bin");write(root/"scene.gltf",remote);
    ticket=runtime.begin(root/"scene.gltf");require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"remote resource accepted");runtime.release(ticket);
    write(root/"scene.gltf",scene());write(root/"large.gltf",std::string(4*1024*1024+1,' '));ticket=runtime.begin(root/"large.gltf");
    require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"metadata file budget ignored");runtime.release(ticket);
    ticket=runtime.begin(root/"scene.gltf");runtime.prepared(ticket);runtime.release(ticket);
    const auto validCatalog=catalog(root);write(root/"catalog.json",validCatalog);ticket=runtime.begin(root/"catalog.json");runtime.prepared(ticket);
    snapshot=octaryn::scene_loading::snapshot(runtime.owner,ticket);
    require(snapshot && snapshot->instances.size()==2 && snapshot->instances[1].transform[12]==5 && snapshot->instanced_triangles==2,
            "SceneCatalog metadata lost original instances");runtime.release(ticket);snapshot.reset();
    auto stale=validCatalog;stale.replace(stale.find("\"version\":4"),11,"\"version\":3");
    write(root/"catalog.json",stale);ticket=runtime.begin(root/"catalog.json");
    require(runtime.wait(ticket).preparation==OCTARYN_SCENE_LOADING_FAILED,"stale forward-material catalog admitted");runtime.release(ticket);
    auto gap=validCatalog;const auto gapAt=gap.find("\"first_triangle\":0");gap.replace(gapAt,18,"\"first_triangle\":1");
    write(root/"catalog.json",gap);ticket=runtime.begin(root/"catalog.json");runtime.failed(ticket,"gap or overlap");runtime.release(ticket);
    write(root/"catalog.json",validCatalog);bin[0]=2;write(root/"scene.bin",bin);ticket=runtime.begin(root/"catalog.json");
    runtime.failed(ticket,"digest mismatch");runtime.release(ticket);
    std::cout<<"scene_loading_probe assertions="<<assertions<<" original_instances=2 shared_meshes=1 publication=0 decode_payload_bytes=0\n";
}
void selected(const fs::path& source) {
    Runtime runtime(source.parent_path());const auto start=std::chrono::steady_clock::now();const auto ticket=runtime.begin(source);
    const auto submitted=std::chrono::steady_clock::now();runtime.prepared(ticket);const auto done=std::chrono::steady_clock::now();
    const auto snapshot=octaryn::scene_loading::snapshot(runtime.owner,ticket);
    const auto milliseconds=[](const auto elapsed){return std::chrono::duration<double,std::milli>(elapsed).count();};
    std::cout<<"scene_loading_selected meshes="<<snapshot->meshes<<" instances="<<snapshot->instances.size()<<" unique_triangles="<<snapshot->unique_triangles
        <<" instanced_triangles="<<snapshot->instanced_triangles<<" retained_bytes="<<snapshot->retained_bytes<<" begin_ms="<<milliseconds(submitted-start)
        <<" prepare_ms="<<milliseconds(done-start)<<" publication=0 identity="<<snapshot->identity<<'\n';runtime.release(ticket);
}
}
int main(int argc,char** argv) {
    try {synthetic();if(argc==3 && std::string(argv[1])=="--scene")selected(fs::absolute(argv[2]));
        else if(argc!=1)throw std::runtime_error("usage: scene_loading_probe [--scene prepared.gltf|scene-import.json|catalog.json]");return 0;
    }catch(const std::exception& failure) {std::cerr<<"scene_loading_probe_failed "<<failure.what()<<'\n';return 1;}
}
