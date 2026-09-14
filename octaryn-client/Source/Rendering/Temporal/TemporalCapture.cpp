#include "TemporalCapture.h"
#include <filesystem>
#include <fstream>
namespace octaryn::client::rendering {
bool capture_temporal(const WorldTemporal& t,rhi::IDevice* device,rhi::ITexture* scene,
    rhi::ITexture* depth,unsigned slot,const char* capture_path) {
    if(!t.mode)return true;
    const auto& frame=t.targets.at(slot);
    struct Image {const char* name;rhi::ITexture* texture;};
    const Image images[]={{"scene",scene},{"output",frame.output},{"depth",depth},
        {"motion",frame.motion},{"reactive",frame.reactive},{"object",frame.object_motion},{"opaque",frame.opaque}};
    const std::filesystem::path base(reinterpret_cast<const char8_t*>(capture_path));
    auto metadata_path=base;metadata_path+=".temporal.json";
    std::ofstream metadata(metadata_path);
    if(!metadata)return false;
    metadata<<"{\"render_width\":"<<t.width<<",\"render_height\":"<<t.height
        <<",\"dynamic_active\":"<<t.resolution.active<<",\"render_scale\":"<<t.resolution.scale
        <<",\"mode\":"<<t.mode<<",\"sharpening\":"<<t.sharpening<<",\"sharpness\":"<<t.sharpness
        <<",\"minimum_scale\":"<<t.resolution.minimum<<",\"maximum_scale\":"<<t.resolution.maximum
        <<",\"target_fps\":"<<t.resolution.target_fps
        <<",\"scene_domain\":\"tone_mapped_linear\",\"output_domain\":\"tone_mapped_linear\",\"opaque_domain\":\"hdr_linear\",\"jitter\":["
        <<t.jitter.x<<","<<t.jitter.y<<"],\"reset\":"<<t.reset<<",\"images\":[";
    bool first=true;
    for(const auto& image:images) {
        Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
        if(!image.texture || SLANG_FAILED(device->readTexture(image.texture,0,0,bytes.writeRef(),&layout)) || !bytes)return false;
        auto path=base;path+=".";path+=image.name;path+=".bin";
        std::ofstream file(path,std::ios::binary);
        file.write(static_cast<const char*>(bytes->getBufferPointer()),static_cast<std::streamsize>(bytes->getBufferSize()));
        if(!file)return false;
        const auto& desc=image.texture->getDesc();
        if(!first)metadata<<",";first=false;
        metadata<<"{\"name\":\""<<image.name<<"\",\"width\":"<<desc.size.width<<",\"height\":"<<desc.size.height
            <<",\"format\":\""<<rhi::getFormatInfo(desc.format).name<<"\",\"row_pitch\":"<<layout.rowPitch
            <<",\"col_pitch\":"<<layout.colPitch<<",\"bytes\":"<<bytes->getBufferSize()<<"}";
    }
    metadata<<"]}\n";return static_cast<bool>(metadata);
}
}
