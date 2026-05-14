#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "core/log.h"
#include "core/persistence/state.h"

namespace persistence_json {

template <typename T>
bool read_json_file(const std::filesystem::path& path, T& value)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    const std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto error = glz::read<k_json_read_opts>(value, std::string_view{buffer.data(), buffer.size()});
    if (error)
    {
        oct_log_errorf("Failed to parse json file: %s", path.string().c_str());
        return false;
    }
    return true;
}

template <typename T>
bool write_json_file_atomic(const std::filesystem::path& path, const T& value)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        oct_log_errorf("Failed to create save directories: %s", path.parent_path().string().c_str());
        return false;
    }
    std::string buffer{};
    auto write_error = glz::write<k_json_write_opts>(value, buffer);
    if (write_error)
    {
        oct_log_errorf("Failed to encode json file: %s", path.string().c_str());
        return false;
    }
    const std::filesystem::path temp_path = path.string() + ".tmp";
    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            oct_log_errorf("Failed to open temp save file: %s", temp_path.string().c_str());
            return false;
        }
        file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (!file.good())
        {
            oct_log_errorf("Failed to write temp save file: %s", temp_path.string().c_str());
            return false;
        }
    }
    std::filesystem::rename(temp_path, path, error);
    if (error)
    {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temp_path, path, error);
    }
    if (error)
    {
        oct_log_errorf("Failed to replace save file: %s", path.string().c_str());
        std::filesystem::remove(temp_path, error);
        return false;
    }
    return true;
}

} // namespace persistence_json
