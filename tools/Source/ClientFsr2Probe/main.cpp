#include "Fsr2.h"
#include "WorldTemporal.h"
#include "RhiShader.h"
#include <slang-com-ptr.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

void qualify_temporal_inputs(rhi::IDevice*, rhi::ICommandQueue*);
void qualify_jitter_raster(rhi::IDevice*, rhi::ICommandQueue*, unsigned);
namespace {
using namespace octaryn::client::rendering;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Debug final : rhi::IDebugCallback {
    std::atomic<unsigned> errors{}, warnings{};
    void SLANG_MCALL handleMessage(rhi::DebugMessageType type, rhi::DebugMessageSource,
        const char* text) noexcept override {
        if (type == rhi::DebugMessageType::Error) ++errors;
        if (type == rhi::DebugMessageType::Warning) ++warnings;
        std::fprintf(stderr, "fsr2_rhi severity=%s %s\n", type == rhi::DebugMessageType::Error ? "error" :
            type == rhi::DebugMessageType::Warning ? "warning" : "info", text ? text : "");
    }
};
float half(uint16_t value) {
    const unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
    require(exponent != 31, "FSR2 output contains nonfinite pixels");
    return (value & 32768 ? -1.0f : 1.0f) * std::ldexp(float(exponent ? fraction + 1024 : fraction),
        exponent ? int(exponent) - 25 : -24);
}
using Context = std::unique_ptr<Fsr2Context, decltype(&destroy_fsr2)>;
struct Fixture {
    Debug& debug;
    Slang::ComPtr<rhi::IDevice> device;
    Slang::ComPtr<rhi::ICommandQueue> queue;
    Slang::ComPtr<rhi::ITexture> color, depth, motion, mask, output;
    WorldTemporal edge_temporal;
    unsigned display;
    explicit Fixture(Debug& debug_, rhi::DeviceType backend, unsigned size) : debug(debug_), display(size) {
        rhi::DeviceDesc desc{};
        desc.deviceType = backend;
        desc.enableValidation = true;
        desc.debugCallback = &debug;
        if (backend == rhi::DeviceType::Vulkan) {
            desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
            desc.slang.targetProfile = "spirv_1_3";
        }
        require(SLANG_SUCCEEDED(rhi::getRHI()->createDevice(desc, device.writeRef())), "FSR2 headless device failed");
        require(SLANG_SUCCEEDED(device->getQueue(rhi::QueueType::Graphics, queue.writeRef())), "FSR2 queue failed");
        qualify_temporal_inputs(device, queue);
        qualify_jitter_raster(device, queue, display);
        std::printf("fsr2_device backend=slang_rhi api=%s adapter=%s surface=none\n",
            device->getInfo().apiName, device->getInfo().adapterName);
        std::vector<float> depth_data(64 * 64, 0.5f), motion_data(64 * 64 * 2, 0);
        std::vector<uint8_t> mask_data(64 * 64, 0);
        depth = texture(rhi::Format::R32Float, 64, depth_data.data(), 4);
        motion = texture(rhi::Format::RG32Float, 64, motion_data.data(), 8);
        mask = texture(rhi::Format::R8Unorm, 64, mask_data.data(), 1);
        output = texture(rhi::Format::RGBA16Float, display, nullptr, 8);
        set_color(false, false);
    }
    Slang::ComPtr<rhi::ITexture> texture(rhi::Format format, unsigned size, const void* data, unsigned stride) {
        rhi::TextureDesc desc{};
        desc.size = {size, size, 1}; desc.format = format;
        desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::UnorderedAccess |
            rhi::TextureUsage::CopySource | rhi::TextureUsage::CopyDestination;
        desc.defaultState = rhi::ResourceState::ShaderResource;
        rhi::SubresourceData init{};
        init.data = data; init.rowPitch = size * stride; init.slicePitch = size * size * stride;
        auto result = device->createTexture(desc, data ? &init : nullptr);
        require(result != nullptr, "FSR2 fixture texture failed");
        return result;
    }
    void set_color(bool checker, bool black) {
        std::vector<std::array<float, 4>> data(64 * 64);
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
            const float value = black ? 0.0f : checker ? (((x / 3 + y / 3) & 1) ? 3.0f : 0.1f) : 1.0f;
            data[y * 64 + x] = {value, value * 0.5f, value * 0.25f, 1};
        }
        color = texture(rhi::Format::RGBA32Float, 64, data.data(), 16);
    }
    void set_edge(unsigned frame,bool mapped) {
        const auto jitter = fsr2_jitter(frame, 64, display);
        std::vector<std::array<float,4>> data(64 * 64);
        std::vector<float> depths(64 * 64);
        for (unsigned y=0; y<64; ++y) for (unsigned x=0; x<64; ++x) {
            const bool sky = float(y)+0.5f-jitter.y < 32+0.2f*(float(x)+0.5f-jitter.x-32);
            data[y*64+x] = sky ? std::array<float,4>{2.5f,5.5f,999,1} : std::array<float,4>{0.03f,0.08f,0.01f,1};
            depths[y*64+x] = sky ? 1.0f : 0.999f;
        }
        color = texture(rhi::Format::RGBA32Float,64,data.data(),16);
        depth = texture(rhi::Format::R32Float,64,depths.data(),4);
        if(mapped) {
            auto& t=edge_temporal;t.mode=1;t.width=t.height=64;t.camera={0,0,0,0,0,1.04719755f};
            t.history.commit(t.camera,64,64);t.jitter=jitter;t.reset=frame==0;
            if(!t.inputs)require(create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Temporal/Inputs.slang","main",t.inputs),"edge temporal pipeline");
            auto& f=t.targets[0];f.opaque=texture(rhi::Format::RGBA32Float,64,data.data(),16);
            std::vector<std::array<float,4>> zero(64*64);
            if(!f.object_motion)f.object_motion=texture(rhi::Format::RGBA32Float,64,zero.data(),16);
            f.motion=motion;f.reactive=mask;
            f.object_view=f.object_motion->createView({});f.opaque_view=f.opaque->createView({});
            f.motion_view=motion->createView({});f.reactive_view=mask->createView({});
        }
    }
    Context create(bool hdr=true,bool dynamic=false) {
        Context undersized(create_fsr2(device, {63,63,display,display,true,false,false}), destroy_fsr2);
        require(!undersized, "FSR2 accepted a context without luminance mip 5");
        Context thin(create_fsr2(device, {64,1,display,display,true,false,false}), destroy_fsr2);
        require(!thin, "FSR2 accepted a zero-height half-size luminance texture");
        Context context(create_fsr2(device, {64,64,display,display,hdr,false,false,dynamic}), destroy_fsr2);
        require(context != nullptr, "FSR2 SDK context creation failed");
        require(fsr2_gpu_bytes(context.get()) > uint64_t(display) * display * 8, "FSR2 history not allocated");
        return context;
    }
    std::vector<float> run(Fsr2Context* context, unsigned frame, bool reset, bool sharpen = false,bool mapped=false,unsigned render_size=64) {
        auto commands = queue->createCommandEncoder();
        if(mapped) {
            auto color_view=color->createView({});
            require(prepare_temporal(edge_temporal,commands,0,depth,color_view,edge_temporal.targets[0].object_view),"actual edge temporal preparation");
        }
        auto jitter = fsr2_jitter(frame, render_size, display);
        require(std::abs(jitter.x) <= 0.5f && std::abs(jitter.y) <= 0.5f, "SDK jitter range");
        Fsr2Dispatch desc{};
        desc.color = color; desc.depth = depth; desc.motion_vectors = motion;
        desc.reactive = mask; desc.transparency = mask; desc.output = output;
        desc.render_width = desc.render_height = render_size;
        desc.jitter_x = jitter.x; desc.jitter_y = jitter.y;
        desc.delta_ms = 1000.0f / 60; desc.vertical_fov = 1.04719755f;
        desc.reset = reset; desc.sharpen = sharpen; desc.sharpness = 0.4f;
        require(dispatch_fsr2(context, commands, desc), fsr2_error(context));
        auto buffer = commands->finish();
        require(buffer != nullptr && SLANG_SUCCEEDED(queue->submit(buffer)), "FSR2 queue submission failed");
        require(SLANG_SUCCEEDED(queue->waitOnHost()), "FSR2 GPU completion failed");
        Slang::ComPtr<ISlangBlob> blob;
        rhi::SubresourceLayout layout{};
        require(SLANG_SUCCEEDED(device->readTexture(output, 0, 0, blob.writeRef(), &layout)),
            "FSR2 actual texture readback failed");
        require(layout.colPitch == 8 && layout.rowPitch >= display * 8, "FSR2 HDR readback layout");
        std::vector<float> result(display * display * 3);
        for (unsigned y = 0; y < display; ++y) for (unsigned x = 0; x < display; ++x) {
            const auto* pixel = reinterpret_cast<const uint16_t*>(static_cast<const char*>(blob->getBufferPointer()) + y * layout.rowPitch + x * 8);
            for (unsigned c = 0; c < 3; ++c) result[(y * display + x) * 3 + c] = half(pixel[c]);
        }
        require(debug.errors == 0, "FSR2 RHI/backend validation errors");
        return result;
    }
};
void qualify(Debug& debug, rhi::DeviceType backend, unsigned display) {
    Fixture fixture(debug, backend, display);
    auto context = fixture.create();
    std::vector<float> image;
    for (unsigned frame = 0; frame < 9; ++frame) {
        image = fixture.run(context.get(), frame, frame == 0, frame == 8);
        float error = 0; size_t location = 0;
        const float reference[] = {1.0f, 0.5f, 0.25f};
        for (size_t i = 0; i < image.size(); ++i) if (std::abs(image[i] - reference[i % 3]) > error) {
            error = std::abs(image[i] - reference[i % 3]); location = i;
        }
        std::printf("fsr2_constant frame=%u sharpen=%u max_error=%g pixel=(%zu,%zu) channel=%zu actual=%g\n",
            frame, frame == 8, error, location / 3 % display, location / 3 / display, location % 3, image[location]);
        require(error < 0.0025f, "FSR2 constant HDR frame preservation failed");
    }
    float maximum_error = 0;
    const float expected[3] = {1.0f, 0.5f, 0.25f};
    for (size_t i = 0; i < image.size(); ++i) maximum_error = std::max(maximum_error, std::abs(image[i] - expected[i % 3]));
    const auto center = (display / 2 * display + display / 2) * 3;
    std::printf("fsr2_hdr display=%u max_error=%g center=(%g,%g,%g) corner=(%g,%g,%g)\n",
        display, maximum_error, image[center], image[center+1], image[center+2], image[0], image[1], image[2]);
    require(maximum_error < 0.0025f, "FSR2 constant HDR preservation failed");
    fixture.set_color(true, false);
    auto reset_checker = fixture.run(context.get(), 8, true);
    auto history_checker = fixture.run(context.get(), 9, false);
    unsigned temporal_changes = 0;
    for (size_t i = 0; i < reset_checker.size(); ++i) temporal_changes += reset_checker[i] != history_checker[i];
    require(temporal_changes > 0, "FSR2 temporal/jitter sequence produced no reconstruction change");
    fixture.set_color(false, true);
    auto reset_black = fixture.run(context.get(), 10, true);
    auto fresh = fixture.create();
    auto fresh_black = fixture.run(fresh.get(), 10, true);
    require(reset_black == fresh_black, "FSR2 reset retained prior scene history");
    for (float value : reset_black) require(std::abs(value) < 0.001f, "FSR2 black reset has ghost energy");
    std::printf("fsr2_case display=%u render=64 dispatches=13 hdr_error=%g temporal_changed=%u reset=fresh bytes=%llu\n",
        display, maximum_error, temporal_changes, static_cast<unsigned long long>(fsr2_gpu_bytes(context.get())));
    fixture.set_color(false, false);
    auto dynamic_context=fixture.create(true,true);
    constexpr unsigned sizes[]={64,48,32,56,64,40,64};
    for(unsigned frame=0;frame<std::size(sizes);++frame) {
        const auto dynamic_image=fixture.run(dynamic_context.get(),frame,frame==0,false,false,sizes[frame]);
        float error=0;
        for(size_t i=0;i<dynamic_image.size();++i)error=std::max(error,std::abs(dynamic_image[i]-expected[i%3]));
        require(error<.0025f,"FSR2 changing render extent corrupted constant history");
        std::printf("fsr2_dynamic display=%u render=%u frame=%u reset=%u max_error=%g allocation=64\n",
            display,sizes[frame],frame,frame==0,error);
    }
    for(bool mapped:{false,true}) {
      auto edge_context=fixture.create(!mapped);
      for(unsigned frame=0;frame<12;++frame) {
        fixture.set_edge(frame,mapped);
        auto edge=fixture.run(edge_context.get(),frame,frame==0,false,mapped);
        if(!mapped)for(auto& value:edge)value/=1+value;
        float minimum=1e20f,maximum=-1e20f,residual=0;size_t location=0;
        float dark[3]={.03f,.08f,.01f},bright[3]={2.5f,5.5f,999};
        for(unsigned c=0;c<3;++c){dark[c]/=1+dark[c];bright[c]/=1+bright[c];}
        float square=0;for(unsigned c=0;c<3;++c)square+=(bright[c]-dark[c])*(bright[c]-dark[c]);
        for(size_t p=0;p<edge.size();p+=3) {
            float along=0;for(unsigned c=0;c<3;++c)along+=(edge[p+c]-dark[c])*(bright[c]-dark[c]);
            along/=square;
            for(unsigned c=0;c<3;++c) {
                minimum=std::min(minimum,edge[p+c]);maximum=std::max(maximum,edge[p+c]);
                const float error=std::abs(edge[p+c]-(dark[c]+along*(bright[c]-dark[c])));
                if(error>residual){residual=error;location=p;}
            }
        }
        std::printf("fsr2_edge display=%u mapped=%u frame=%u min=%g max=%g chroma_residual=%g pixel=(%zu,%zu) rgb=(%g,%g,%g)\n",
            display,mapped,frame,minimum,maximum,residual,location/3%display,location/3/display,
            edge[location],edge[location+1],edge[location+2]);
        if(mapped) {
            require(minimum>=0 && maximum<=1,"mapped FSR2 edge left the display-linear range");
            require(residual<.0025f,"mapped FSR2 edge acquired false chroma");
        } else if(frame==11)require(residual>.1f,"HDR sky negative control failed to reproduce hue fringe");
      }
    }
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Usage: octaryn_client_fsr2_probe vulkan|dx12|metal");
        const std::string name = argv[1];
        require(name == "vulkan" || name == "dx12" || name == "metal", "Unknown backend");
        auto backend = name == "vulkan" ? rhi::DeviceType::Vulkan : name == "dx12" ? rhi::DeviceType::D3D12 : rhi::DeviceType::Metal;
        Debug debug;
        rhi::DebugLayerOptions options{}; options.coreValidation = true; options.required = true;
        require(SLANG_SUCCEEDED(rhi::getRHI()->setDebugLayerOptions(options)), "RHI validation setup failed");
        qualify(debug, backend, 64);
        qualify(debug, backend, 96);
        require(debug.errors == 0, "FSR2 teardown validation errors");
        std::printf("fsr2_validation=passed errors=%u warnings=%u api=%s\n", debug.errors.load(), debug.warnings.load(), argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fsr2_validation=failed %s\n", error.what());
        return 1;
    }
}
