#include "AppSettings.h"
#include "RuntimeSettings.h"
#include "RuntimeControls.h"
#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cmath>
#include <limits>

struct SavedMode {unsigned upscalerMode=999;};
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
void write(const std::filesystem::path& path,const std::string& value) {
  std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<value;
  require(bool(file),"cannot write isolated settings fixture");
}
}
int main(int argc,char** argv) {
  try {
    require(argc==2,"usage: settings_probe isolated-directory");
    const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
    const auto path=root/"client-settings.json";const auto utf8=path.generic_u8string();
    require(SDL_setenv_unsafe("OCTARYN_CLIENT_SETTINGS_PATH",reinterpret_cast<const char*>(utf8.c_str()),1)==0,
        "cannot select isolated settings file");
    app_settings settings{};app_settings_default(&settings);
    require(settings.upscaler_mode==0,"upscaler default is not Off");
    require(settings.ray_tracing_enabled==1,"ray tracing default is not On");
    require(settings.render_distance==4,"new settings world radius is not 4");
    require(settings.gi_voxel_radius==6 && settings.gi_coarse_radius==128 && settings.lighting_quality==2 &&
        settings.shadow_distance==1024 && settings.reflection_distance==1024,"lighting defaults changed");
    for(unsigned quality=0;quality<4;++quality)for(unsigned mode=0;mode<4;++mode) {
      write(path,"{\"version\":13,\"giVoxelRadius\":"+std::to_string(mode&1?32:0)+
          ",\"giCoarseRadius\":"+std::to_string(mode&2?1024:0)+",\"shadowDistance\":"+
          std::to_string(mode&1?1024:0)+",\"reflectionDistance\":"+std::to_string(mode&2?1024:0)+
          ",\"lightingQuality\":"+std::to_string(quality)+",\"rasterSunShadows\":0}");
      runtime_controls lighting{};
      require(runtime_settings_load(nullptr,&lighting),"lighting load failed");
      require(lighting.gi_voxel_radius==(mode&1?32:0) && lighting.gi_coarse_radius==(mode&2?1024:0) &&
          lighting.shadow_distance==(mode&1?1024:0) && lighting.reflection_distance==(mode&2?1024:0) &&
          lighting.lighting_quality==quality && lighting.raster_sun_shadows==0,"saved lighting preference replaced");
      require(runtime_settings_save(nullptr,&lighting),"lighting save failed");
      runtime_controls restored{};
      require(runtime_settings_load(nullptr,&restored) && restored.gi_voxel_radius==lighting.gi_voxel_radius &&
          restored.gi_coarse_radius==lighting.gi_coarse_radius && restored.shadow_distance==lighting.shadow_distance &&
          restored.reflection_distance==lighting.reflection_distance && restored.lighting_quality==quality &&
          restored.raster_sun_shadows==0,"independent lighting settings did not roundtrip");
    }
    write(path,R"({"version":13,"giVoxelRadius":65535,"giCoarseRadius":65535,"shadowDistance":65535,"reflectionDistance":65535,"lightingQuality":255})");
    runtime_controls limits{};
    require(runtime_settings_load(nullptr,&limits) && limits.gi_voxel_radius==32 && limits.gi_coarse_radius==1024 &&
        limits.shadow_distance==1024 && limits.reflection_distance==1024 && limits.lighting_quality==2,
        "lighting saved limits disagree with menu limits");
    write(path,R"({"version":11,"shadowDistance":0,"reflectionDistance":0,"rasterSunShadows":0})");
    require(runtime_settings_load(nullptr,&limits) && limits.shadow_distance==1024 && limits.reflection_distance==1024 &&
        limits.raster_sun_shadows==1,"legacy unlimited trace distance migration changed");
    write(path,R"({"version":13,"giVoxelRadius":16,"giCoarseRadius":513,"shadowDistance":257,"reflectionDistance":769,"lightingQuality":3})");
    require(runtime_settings_load(nullptr,&limits) && runtime_settings_save(nullptr,&limits) &&
        runtime_settings_load(nullptr,&limits) && limits.gi_voxel_radius==16 && limits.gi_coarse_radius==513 &&
        limits.shadow_distance==257 && limits.reflection_distance==769 && limits.lighting_quality==3,
        "intermediate saved lighting preferences were rounded or replaced");
    write(path,R"({"version":13})");
    runtime_controls distance{};
    require(runtime_settings_load(nullptr,&distance) && distance.render_distance==4,
        "omitted world radius did not default to 4");
    for(int radius:{4,16,32}) {
      write(path,"{\"version\":13,\"renderDistance\":"+std::to_string(radius)+"}");
      require(runtime_settings_load(nullptr,&distance) && distance.render_distance==radius,
          "explicit saved world radius was replaced");
      require(runtime_settings_save(nullptr,&distance) && runtime_settings_load(nullptr,&distance) &&
          distance.render_distance==radius,"world radius did not roundtrip");
    }
    for(unsigned enabled:{0u,1u}) {
      runtime_controls ray{};ray.ray_tracing_enabled=static_cast<uint8_t>(enabled);
      ray.ray_tracing_available=0;
      require(runtime_settings_save(nullptr,&ray),"ray tracing save failed");
      runtime_controls restored{};restored.ray_tracing_available=1;
      require(runtime_settings_load(nullptr,&restored) && restored.ray_tracing_enabled==enabled,
          "ray tracing preference roundtrip failed");
      require(restored.ray_tracing_available==1,"settings changed runtime graphics capability");
    }
    write(path,R"({"version":9})");
    runtime_controls legacy{};
    require(runtime_settings_load(nullptr,&legacy) && legacy.ray_tracing_enabled==1,
        "legacy settings did not enable ray tracing default");
    write(path,R"({"version":10,"rayTracingEnabled":"yes"})");
    legacy.ray_tracing_enabled=0;
    require(!runtime_settings_load(nullptr,&legacy) && legacy.ray_tracing_enabled==0,
        "malformed ray tracing preference changed controls");
    settings.ray_tracing_enabled=255;
    require(app_settings_sanitize(&settings) && settings.ray_tracing_enabled==1,
        "ray tracing native flag was not normalized");
    for(unsigned mode=0;mode<7;++mode) {
      settings.upscaler_mode=static_cast<std::uint8_t>(mode);
      require(app_settings_sanitize(&settings) && settings.upscaler_mode==mode,"valid upscaler mode clamped");
      runtime_controls controls{};controls.upscaler_mode=static_cast<std::uint8_t>(mode);
      require(runtime_settings_save(nullptr,&controls)!=0,"upscaler save failed");
      std::ifstream file(path,std::ios::binary);
      const std::string json((std::istreambuf_iterator<char>(file)),{});
      SavedMode saved;constexpr glz::opts options{.error_on_unknown_keys=false};
      require(!glz::read<options>(saved,json) && saved.upscalerMode==mode,"saved mode differs from chosen mode");
      runtime_controls loaded{};loaded.upscaler_mode=255;
      require(runtime_settings_load(nullptr,&loaded) && loaded.upscaler_mode==mode,"upscaler save/load roundtrip");
    }
    for(unsigned invalid:{7u,255u}) {
      settings.upscaler_mode=static_cast<std::uint8_t>(invalid);
      require(app_settings_sanitize(&settings) && settings.upscaler_mode==0,"invalid native mode did not default Off");
    }
    runtime_controls loaded{};
    for(const char* json:{"{\"version\":1}","{\"version\":8,\"upscalerMode\":7}",
        "{\"version\":8,\"upscalerMode\":4294967295}"}) {
      write(path,json);loaded.upscaler_mode=5;
      require(runtime_settings_load(nullptr,&loaded) && loaded.upscaler_mode==0,
          "legacy omitted or invalid persisted mode did not default Off");
    }
    for(const char* json:{"{\"version\":8,\"upscalerMode\":-1}","{\"version\":8,\"upscalerMode\":\"Quality\"}"}) {
      write(path,json);loaded.upscaler_mode=3;
      require(!runtime_settings_load(nullptr,&loaded) && loaded.upscaler_mode==3,
          "malformed mode changed live settings before validation");
    }
    write(path,"{\"version\":8,\"upscalerMode\":2}");
    require(runtime_settings_load(nullptr,&loaded)&&loaded.fsr_sharpening==1&&loaded.fsr_sharpness==.2f&&loaded.fsr_target_fps==60,
        "legacy sharpening defaults missing");
    loaded.fsr_sharpening=1;loaded.fsr_sharpness=.73f;loaded.fsr_render_scale=.725f;
    loaded.fsr_dynamic_resolution=1;loaded.fsr_min_scale=.45f;loaded.fsr_max_scale=.85f;loaded.fsr_target_fps=144;
    require(runtime_settings_save(nullptr,&loaded),"FSR settings save failed");
    runtime_controls restored{};
    require(runtime_settings_load(nullptr,&restored)&&restored.fsr_sharpening==1&&restored.fsr_dynamic_resolution==1&&
        restored.fsr_sharpness==.73f&&restored.fsr_render_scale==.725f&&restored.fsr_min_scale==.45f&&restored.fsr_max_scale==.85f&&
        restored.fsr_target_fps==144,"FSR controls roundtrip mismatch");
    settings.fsr_sharpness=std::numeric_limits<float>::quiet_NaN();settings.fsr_render_scale=2;
    settings.fsr_min_scale=.9f;settings.fsr_max_scale=.4f;settings.fsr_target_fps=1;
    require(app_settings_sanitize(&settings)&&settings.fsr_sharpness==.2f&&settings.fsr_render_scale==1&&
        settings.fsr_min_scale==.9f&&settings.fsr_max_scale==.9f&&settings.fsr_target_fps==30,"FSR invalid values not bounded");
    loaded.present_mode_index=2;loaded.frame_cap_fps=144;
    require(runtime_settings_save(nullptr,&loaded),"frame pacing save failed");
    runtime_controls pacing{};
    require(runtime_settings_load(nullptr,&pacing)&&pacing.present_mode_index==2&&pacing.frame_cap_fps==144,
        "frame pacing roundtrip mismatch");
    settings.present_mode_index=9;settings.frame_cap_fps=12;
    require(app_settings_sanitize(&settings)&&settings.present_mode_index==2&&settings.frame_cap_fps==30,
        "frame pacing bounds not applied");
    settings.frame_cap_fps=1;
    require(app_settings_sanitize(&settings)&&settings.frame_cap_fps==1,"display refresh cap was clamped");
    write(path,"{\"version\":10}");
    runtime_controls legacy_pacing{};
    require(runtime_settings_load(nullptr,&legacy_pacing)&&legacy_pacing.frame_cap_fps==0&&legacy_pacing.present_mode_index==0,
        "legacy settings did not default uncapped presentation");
    loaded.upscaler_mode=255;
    require(runtime_settings_save(nullptr,&loaded),"invalid native settings save failed");
    require(runtime_settings_load(nullptr,&loaded) && loaded.upscaler_mode==0,"save sanitizer did not persist Off");
    std::printf("settings_upscaler=passed checks=%u modes=7 legacy_default=Off invalid_default=Off roundtrip=exact ray_tracing=passed lighting=passed windows=0 gpu_devices=0\n",checks);
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"settings_upscaler=failed checks=%u reason=%s\n",checks,error.what());return 1;
  }
}
