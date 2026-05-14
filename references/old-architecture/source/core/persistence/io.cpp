#include <zlib.h>

#include "core/persistence/internal.h"

namespace persistence_json {

bool write_gzip_file(const std::filesystem::path& path, std::string_view payload)
{
    gzFile file = gzopen(path.string().c_str(), "wb9");
    if (!file)
    {
        oct_log_errorf("Failed to open gzip export file: %s", path.string().c_str());
        return false;
    }
    const int written = gzwrite(file, payload.data(), (unsigned int) payload.size());
    const int close_result = gzclose(file);
    return written == (int) payload.size() && close_result == Z_OK;
}

bool read_gzip_file(const std::filesystem::path& path, std::string& payload)
{
    gzFile file = gzopen(path.string().c_str(), "rb");
    if (!file)
    {
        oct_log_errorf("Failed to open gzip import file: %s", path.string().c_str());
        return false;
    }
    std::vector<char> buffer(4096);
    payload.clear();
    int bytes = 0;
    while ((bytes = gzread(file, buffer.data(), (unsigned int) buffer.size())) > 0)
    {
        payload.append(buffer.data(), (size_t) bytes);
    }
    const int close_result = gzclose(file);
    return bytes >= 0 && close_result == Z_OK;
}

} // namespace persistence_json
