#include "app/runtime/telemetry.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

constexpr Uint64 kBytesPerGiB = 1024ull * 1024ull * 1024ull;
constexpr Uint64 kKiBPerGiB = 1024ull * 1024ull;
constexpr Uint64 kTelemetrySampleIntervalNs = SDL_NS_PER_SECOND;
constexpr float kTelemetryLoadSmoothing = 0.35f;

typedef struct cpu_load_sample
{
    Uint64 total = 0;
    Uint64 idle = 0;
    bool valid = false;
}
cpu_load_sample_t;

typedef struct gpu_paths
{
    std::filesystem::path busy_percent;
    std::filesystem::path vram_used;
    std::filesystem::path vram_total;
}
gpu_paths_t;

cpu_load_sample_t g_previous_cpu_load = {};
gpu_paths_t g_gpu_paths = {};
main_runtime_telemetry_snapshot_t g_cached_snapshot = {};
Uint64 g_last_telemetry_sample_ns = 0;
bool g_gpu_paths_initialized = false;

auto to_hundredths_gib_from_bytes(Uint64 bytes) -> Uint32
{
    return static_cast<Uint32>((bytes * 100ull + (kBytesPerGiB / 2ull)) / kBytesPerGiB);
}

auto to_hundredths_gib_from_kib(Uint64 kib) -> Uint32
{
    return static_cast<Uint32>((kib * 100ull + (kKiBPerGiB / 2ull)) / kKiBPerGiB);
}

auto parse_first_u64_from_stream(std::istream& stream, Uint64* out_value) -> bool
{
    if (!out_value)
    {
        return false;
    }

    Uint64 value = 0;
    stream >> value;
    if (!stream.fail())
    {
        *out_value = value;
        return true;
    }
    return false;
}

auto read_file_u64(const std::filesystem::path& path, Uint64* out_value) -> bool
{
    if (path.empty() || !out_value)
    {
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    return parse_first_u64_from_stream(file, out_value);
}

auto file_exists(const std::filesystem::path& path) -> bool
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

auto select_gpu_device_paths(const std::filesystem::path& device_path) -> bool
{
    const std::filesystem::path busy_path = device_path / "gpu_busy_percent";
    const std::filesystem::path used_path = device_path / "mem_info_vram_used";
    const std::filesystem::path total_path = device_path / "mem_info_vram_total";
    if (!file_exists(busy_path) || !file_exists(used_path) || !file_exists(total_path))
    {
        return false;
    }

    g_gpu_paths.busy_percent = busy_path;
    g_gpu_paths.vram_used = used_path;
    g_gpu_paths.vram_total = total_path;
    return true;
}

void discover_gpu_paths(void)
{
    g_gpu_paths = {};
    std::error_code error;
    const std::filesystem::path root = "/sys/class/drm";
    std::filesystem::directory_iterator it(root, error);
    const std::filesystem::directory_iterator end;

    for (; it != end; it.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }
        const std::filesystem::path device_path = it->path() / "device";
        if (!std::filesystem::is_directory(device_path, error))
        {
            error.clear();
            continue;
        }
        error.clear();
        if (select_gpu_device_paths(device_path))
        {
            return;
        }
    }
}

auto sample_cpu_ram_used_hundredths_gib(void) -> Uint32
{
    std::ifstream file("/proc/meminfo");
    if (!file.is_open())
    {
        return 0;
    }

    std::string key;
    Uint64 value = 0;
    std::string unit;
    Uint64 total_kib = 0;
    Uint64 available_kib = 0;
    while (file >> key >> value >> unit)
    {
        if (key == "MemTotal:")
        {
            total_kib = value;
        }
        else if (key == "MemAvailable:")
        {
            available_kib = value;
        }
    }

    if (total_kib == 0 || available_kib > total_kib)
    {
        return 0;
    }

    return to_hundredths_gib_from_kib(total_kib - available_kib);
}

auto sample_cpu_load_hundredths(void) -> Uint32
{
    std::ifstream file("/proc/stat");
    if (!file.is_open())
    {
        return 0;
    }

    std::string cpu_label;
    cpu_load_sample_t current = {};
    Uint64 user = 0;
    Uint64 nice = 0;
    Uint64 system = 0;
    Uint64 idle = 0;
    Uint64 iowait = 0;
    Uint64 irq = 0;
    Uint64 softirq = 0;
    Uint64 steal = 0;
    Uint64 guest = 0;
    Uint64 guest_nice = 0;
    file >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
    if (file.fail() || cpu_label != "cpu")
    {
        return 0;
    }

    current.total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
    current.idle = idle + iowait;
    current.valid = true;

    if (!g_previous_cpu_load.valid || current.total <= g_previous_cpu_load.total || current.idle < g_previous_cpu_load.idle)
    {
        g_previous_cpu_load = current;
        return 0;
    }

    const Uint64 delta_total = current.total - g_previous_cpu_load.total;
    const Uint64 delta_idle = current.idle - g_previous_cpu_load.idle;
    g_previous_cpu_load = current;
    if (delta_total == 0 || delta_idle > delta_total)
    {
        return 0;
    }

    const Uint64 delta_busy = delta_total - delta_idle;
    return static_cast<Uint32>((delta_busy * 10000ull + (delta_total / 2ull)) / delta_total);
}

auto sample_gpu_vram_used_hundredths_gib(void) -> Uint32
{
    Uint64 used_bytes = 0;
    if (!read_file_u64(g_gpu_paths.vram_used, &used_bytes))
    {
        return 0;
    }
    return to_hundredths_gib_from_bytes(used_bytes);
}

auto sample_gpu_load_hundredths(void) -> Uint32
{
    Uint64 busy_percent = 0;
    if (!read_file_u64(g_gpu_paths.busy_percent, &busy_percent))
    {
        return 0;
    }
    if (busy_percent > 100ull)
    {
        busy_percent = 100ull;
    }
    return static_cast<Uint32>(busy_percent * 100ull);
}

auto smooth_load(Uint32 previous, Uint32 sample) -> Uint32
{
    if (previous == 0u)
    {
        return sample;
    }
    const float smoothed = static_cast<float>(previous) +
        (static_cast<float>(sample) - static_cast<float>(previous)) * kTelemetryLoadSmoothing;
    return static_cast<Uint32>(smoothed + 0.5f);
}

} // namespace

void app_runtime_telemetry_init(void)
{
    if (!g_gpu_paths_initialized)
    {
        discover_gpu_paths();
        g_gpu_paths_initialized = true;
    }
    g_previous_cpu_load = {};
    g_cached_snapshot = {};
    g_last_telemetry_sample_ns = 0;
    (void) sample_cpu_load_hundredths();
}

main_runtime_telemetry_snapshot_t app_runtime_telemetry_sample(void)
{
    if (!g_gpu_paths_initialized)
    {
        app_runtime_telemetry_init();
    }

    const Uint64 now_ns = SDL_GetTicksNS();
    if (g_last_telemetry_sample_ns != 0u && now_ns - g_last_telemetry_sample_ns < kTelemetrySampleIntervalNs)
    {
        return g_cached_snapshot;
    }

    main_runtime_telemetry_snapshot_t snapshot = {};
    snapshot.cpu_ram_hundredths_gib = sample_cpu_ram_used_hundredths_gib();
    snapshot.gpu_vram_hundredths_gib = sample_gpu_vram_used_hundredths_gib();
    snapshot.cpu_load_hundredths = smooth_load(g_cached_snapshot.cpu_load_hundredths, sample_cpu_load_hundredths());
    snapshot.gpu_load_hundredths = smooth_load(g_cached_snapshot.gpu_load_hundredths, sample_gpu_load_hundredths());
    g_cached_snapshot = snapshot;
    g_last_telemetry_sample_ns = now_ns;
    return snapshot;
}
