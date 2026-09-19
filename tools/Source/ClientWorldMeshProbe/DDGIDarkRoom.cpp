#include "Probe.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

// Sealed dark-room leak localization: any irradiance that appears inside a
// lightproof shell while the sun moves or the camera scrolls is reported with
// the exact probe cell and octahedral direction that carried it.
namespace mesh_probe {
namespace {
using Clock=std::chrono::steady_clock;
constexpr double SettleSeconds=3,SampleSeconds=.1;
constexpr int Shell0=8,Shell1=24,Air0=10,Air1=22,FloorY=2,AirY0=4,AirY1=10,ShellTop=12;

double elapsed(Clock::time_point start) {return std::chrono::duration<double>(Clock::now()-start).count();}
struct Direction {float x{},y{},z{};};
Direction decode_octahedral(unsigned texel,unsigned resolution) {
    const float u=(float(texel%resolution)+.5f)/float(resolution)*2.f-1.f;
    const float v=(float(texel/resolution)+.5f)/float(resolution)*2.f-1.f;
    float x=u,y=v,z=1.f-std::abs(u)-std::abs(v);
    if(z<0.f) {
        const float nx=(1.f-std::abs(y))*(x>=0.f?1.f:-1.f);
        const float ny=(1.f-std::abs(x))*(y>=0.f?1.f:-1.f);
        x=nx;y=ny;z=0.f;
    }
    const float length=std::sqrt(x*x+y*y+z*z);
    return {x/(length>1e-8f?length:1.f),y/(length>1e-8f?length:1.f),z/(length>1e-8f?length:1.f)};
}
std::uint16_t stone_id(const Fixture& f) {
    const std::string wanted="octaryn.basegame.block.stone";
    for(unsigned i=0;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return std::uint16_t(i);
    require(false,"dark room stone material missing");return 0;
}
std::uint16_t foliage_id(const Fixture& f) {
    const std::string wanted="octaryn.basegame.block.leaves";
    for(unsigned i=0;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return std::uint16_t(i);
    require(false,"dark room leaves material missing");return 0;
}

class DarkRoom {
    WorldRenderer& r;
    std::uint64_t serial{};
    std::ofstream csv;
    // Interior/exterior probe indices and their enclosing buffer spans.
    std::vector<unsigned> interior,exterior;
    unsigned interiorBegin{},interiorEnd{},exteriorBegin{},exteriorEnd{};
public:
    explicit DarkRoom(Fixture& f):r(f.renderer) {
        std::filesystem::path directory="logs/client/ddgi-dark-room";
        if(const char* path=SDL_getenv("OCTARYN_DDGI_DARK_ROOM_OUTPUT"))directory=path;
        std::filesystem::create_directories(directory);
        const std::string backend=r.device->getDeviceType()==rhi::DeviceType::D3D12?"d3d12":"vulkan";
        csv.open(directory/(backend+"-dark-room.csv"));
        require(bool(csv),"dark room evidence file");
        csv<<"phase,elapsed_seconds,interior_max,cell_x,cell_y,cell_z,texel,dir_x,dir_y,dir_z,"
           <<"exterior_max,valid,seeded,pending\n";
    }
    void classify() {
        auto& s=*r.ddgi.fine_volume;
        interiorBegin=unsigned(s.control_data.size());interiorEnd=0;
        exteriorBegin=interiorBegin;exteriorEnd=0;
        for(unsigned i=0;i<s.control_data.size();++i) {
            const auto& cell=s.control_data[i].cell;
            const bool inside=cell[0]>=Air0&&cell[0]<Air1&&cell[1]>=AirY0&&cell[1]<AirY1&&cell[2]>=Air0&&cell[2]<Air1;
            const bool outside=cell[0]>=Air0&&cell[0]<Air1&&cell[2]>=Air0&&cell[2]<Air1&&cell[1]>=ShellTop+4;
            if(inside) {interior.push_back(i);interiorBegin=std::min(interiorBegin,i);interiorEnd=std::max(interiorEnd,i+1);}
            if(outside) {exterior.push_back(i);exteriorBegin=std::min(exteriorBegin,i);exteriorEnd=std::max(exteriorEnd,i+1);}
        }
        require(!interior.empty()&&!exterior.empty(),"dark room missing probe cohorts");
    }
    void scene(float sun,float ambient,double day) {
        auto settings=lighting_settings_default_value();settings.sun_strength=sun;settings.ambient_strength=ambient;
        open_world_renderer_set_lighting(&r,settings);
        WorldSceneSettings scene;scene.day_fraction=day;scene.clouds=false;scene.fog=false;
        open_world_renderer_set_scene(&r,scene);
        std::printf("dark_room_scene sun=%.2f ambient=%.2f day=%.2f direction=(%.3f,%.3f,%.3f) strength=%.3f\n",
            sun,ambient,day,r.sky.light_direction_sky[0],r.sky.light_direction_sky[1],r.sky.light_direction_sky[2],
            r.lighting.sun_strength);
    }
    void frame(const WorldCamera& camera) {
        const auto start=Clock::now();const unsigned slot=r.frame_queue.slot(serial++);
        require(r.frame_queue.wait(slot,10000),"dark room frame wait");r.active_frame=slot;r.frames=serial;
        r.temporal.camera=camera;
        world_renderer_prepare_draw(r,camera);world_block_lights_update(r);
        auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"dark room encoder");
        require(world_local_lighting_prepare(r,commands),"dark room light upload");
        require(world_ddgi_update(r,commands),"dark room DDGI update");
        auto submission=commands->finish();require(submission!=nullptr,"dark room command finish");
        require(r.frame_queue.submit(r.queue,submission,slot),"dark room submit");
        require(r.frame_queue.wait(slot,10000),"dark room GPU frame");
        std::this_thread::sleep_until(start+std::chrono::microseconds(16667));
    }
    struct Reading {
        float interiorMax{},exteriorMax{};unsigned worstIndex{},worstTexel{};unsigned valid{},seeded{},pending{};
    };
    Reading read() {
        auto& s=*r.ddgi.fine_volume;
        const unsigned texels=s.config.irradiance_resolution*s.config.irradiance_resolution;
        const std::size_t interiorCount=std::size_t(interiorEnd-interiorBegin);
        std::vector<DDGIProbe> probes(interiorCount);
        std::vector<std::array<float,4>> inside(interiorCount*texels);
        checked(r.device->readBuffer(s.probes,std::size_t(interiorBegin)*sizeof(DDGIProbe),
            probes.size()*sizeof(DDGIProbe),probes.data()),"dark room probe readback");
        checked(r.device->readBuffer(s.irradiance,interiorBegin*texels*sizeof(inside[0]),
            inside.size()*sizeof(inside[0]),inside.data()),"dark room interior irradiance readback");
        const std::size_t exteriorCount=std::size_t(exteriorEnd-exteriorBegin);
        std::vector<std::array<float,4>> outside(exteriorCount*texels);
        checked(r.device->readBuffer(s.irradiance,std::size_t(exteriorBegin)*texels*sizeof(outside[0]),
            outside.size()*sizeof(outside[0]),outside.data()),"dark room exterior irradiance readback");
        Reading result;
        for(unsigned i:interior) {
            const auto& probe=probes[i-interiorBegin];
            if(probe.metadata[3]==0) {++result.pending;continue;}
            if(probe.metadata[1]==0)++result.seeded;else ++result.valid;
            for(unsigned t=0;t<texels;++t) {
                const auto& texel=inside[std::size_t(i-interiorBegin)*texels+t];
                const float energy=std::max({texel[0],texel[1],texel[2]});
                if(energy>result.interiorMax) {result.interiorMax=energy;result.worstIndex=i;result.worstTexel=t;}
            }
        }
        for(unsigned i:exterior)
            for(unsigned t=0;t<texels;++t) {
                const auto& texel=outside[std::size_t(i-exteriorBegin)*texels+t];
                result.exteriorMax=std::max({result.exteriorMax,texel[0],texel[1],texel[2]});
            }
        return result;
    }
    void run(const char* name,float sun,float ambient,double day,const WorldCamera& camera,
        double seconds,bool sweepSun,bool moveCamera) {
        scene(sun,ambient,day);
        const auto start=Clock::now();double next=0,worstWhen=0;Reading worst;
        do {
            const double now=elapsed(start);
            if(sweepSun)scene(sun,ambient,.02+.96*(.5+.5*std::sin(now*1.1)));
            const WorldCamera view=moveCamera?
                WorldCamera{float(camera.x+4*std::sin(now*1.3)),camera.y,float(camera.z+4*std::cos(now*1.3)),
                    camera.yaw,camera.pitch,camera.vertical_fov}:camera;
            frame(view);
            if(now>=next) {
                next=now+SampleSeconds;
                const Reading current=read();
                if(current.interiorMax>worst.interiorMax) {worst=current;worstWhen=now;}
                auto& s=*r.ddgi.fine_volume;
                const auto& cell=s.control_data[current.worstIndex].cell;
                const auto direction=decode_octahedral(current.worstTexel,s.config.irradiance_resolution);
                csv<<name<<','<<now<<','<<current.interiorMax<<','<<cell[0]<<','<<cell[1]<<','<<cell[2]<<','
                   <<current.worstTexel<<','<<direction.x<<','<<direction.y<<','<<direction.z<<','
                   <<current.exteriorMax<<','<<current.valid<<','<<current.seeded<<','<<current.pending<<'\n';
                csv.flush();
            }
        } while(elapsed(start)<seconds);
        auto& s=*r.ddgi.fine_volume;
        const auto& cell=s.control_data[worst.worstIndex].cell;
        const auto direction=decode_octahedral(worst.worstTexel,s.config.irradiance_resolution);
        std::printf("dark_room phase=%s seconds=%.2f interior_max=%.6f cell=(%d,%d,%d) texel=%u "
            "direction=(%.2f,%.2f,%.2f) exterior_max=%.4f valid=%u seeded=%u pending=%u leaked_at=%.2fs\n",
            name,elapsed(start),worst.interiorMax,cell[0],cell[1],cell[2],worst.worstTexel,
            direction.x,direction.y,direction.z,worst.exteriorMax,worst.valid,worst.seeded,worst.pending,worstWhen);
        require(sun<=0.f || worst.exteriorMax>.25f,"dark room exterior never received sunlight; test cannot pass vacuously");
    }
};
}

void ddgi_dark_room_cases(Fixture& f) {
    DarkRoom room(f);
    const auto stone=stone_id(f);
    room.scene(0,.5,0);
    require(world_ray_initialize(f.renderer)&&world_ray_available(f.renderer),"dark room ray scene");
    require(world_local_lighting_initialize(f.renderer)&&world_ddgi_initialize(f.renderer),"dark room DDGI initialization");
    require(open_world_renderer_set_ddgi_range(&f.renderer,16,128),"dark room volume range");
    require(f.renderer.ddgi.fine_volume&&f.renderer.ddgi.fine_volume->available,"dark room fine volume");
    const WorldCamera inside{16,7,16,0,0,1.05f};
    auto source=column(0,0,0,32);
    for(int z=0;z<32;++z)for(int x=0;x<32;++x)put(source,x,FloorY,z,stone);
    for(int z=Shell0;z<Shell1;++z)for(int y=FloorY+1;y<ShellTop;++y)for(int x=Shell0;x<Shell1;++x)
        put(source,x,y,z,(x>=Air0&&x<Air1&&y>=AirY0&&y<AirY1&&z>=Air0&&z<Air1)?0:stone);
    // Reported scene: transmissive foliage sits directly above the sealed roof.
    const auto leaves_id=foliage_id(f);
    for(int z=Shell0;z<Shell1;++z)for(int y=ShellTop+1;y<ShellTop+3;++y)for(int x=Shell0;x<Shell1;++x)
        put(source,x,y,z,leaves_id);
    source.blocks.compact();++source.revision;
    require(open_world_renderer_update(&f.renderer,source),"dark room geometry publication");
    // Drive the async ray builds with explicit prepare submissions until the
    // acceleration snapshot exists; the first DDGI trace dispatch requires it.
    for(unsigned warm=0;warm<600;++warm) {
        const unsigned slot=f.renderer.frame_queue.slot(f.renderer.frames);
        require(f.renderer.frame_queue.wait(slot,10000),"dark room warmup wait");
        f.renderer.active_frame=slot;++f.renderer.frames;
        world_renderer_prepare_draw(f.renderer,inside);
        auto commands=f.renderer.queue->createCommandEncoder();
        require(commands!=nullptr,"dark room warmup encoder");
        require(world_ray_prepare(f.renderer,commands,slot),"dark room warmup ray prepare");
        auto submission=commands->finish();
        require(submission!=nullptr,"dark room warmup finish");
        require(f.renderer.frame_queue.submit(f.renderer.queue,submission,slot),"dark room warmup submit");
        require(f.renderer.frame_queue.wait(slot,10000),"dark room warmup fence");
        const auto rays=world_ray_stats(f.renderer);
        if(rays.ready_columns>=1 && rays.pending_columns==0 && rays.active_jobs==0)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    const auto warmed=world_ray_stats(f.renderer);
    require(warmed.ready_columns>=1 && warmed.pending_columns==0 && warmed.active_jobs==0,
        "dark room ray acceleration never became ready");
    // One frame scrolls the volume and assigns cells before cohorts are read.
    room.frame(inside);
    room.classify();
    // Warm up with the sun off: the sealed interior must be near black.
    room.run("dark_baseline",0,.5,0,inside,SettleSeconds,false,false);
    const auto baseline=room.read();
    require(baseline.interiorMax<.02f,"sealed dark room was not dark before the sun was enabled");
    room.run("sun_static",2,0,.5,inside,SettleSeconds,false,false);
    room.run("sun_sweep",2,0,.5,inside,SettleSeconds,true,false);
    room.run("camera_scroll",2,0,.5,inside,SettleSeconds,false,true);
    room.run("sun_and_scroll",2,0,.5,inside,SettleSeconds,true,true);
    const auto limit=std::max(baseline.interiorMax*3.f,.02f);
    const auto after=room.read();
    if(after.interiorMax>limit) {
        std::fprintf(stderr,"dark_room_leak interior_max=%.6f limit=%.6f baseline=%.6f exterior=%.4f\n",
            after.interiorMax,limit,baseline.interiorMax,after.exteriorMax);
        throw std::runtime_error("sunlight leaked into the sealed dark room");
    }
    require(f.renderer.debug.errors.load()==0,"dark room graphics validation errors");
    std::puts("ddgi_dark_room=passed sealed_interior_dark=1 sun_static=1 sun_sweep=1 camera_scroll=1 combined=1");
}
}