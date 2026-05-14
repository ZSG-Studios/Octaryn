#pragma once

#include <cstddef>
#include <functional>

#include <glaze/glaze.hpp>

namespace persistence_json {

struct chunk_key {
    int cx;
    int cz;

    auto operator==(const chunk_key& other) const -> bool = default;
};

struct block_key {
    int bx;
    int by;
    int bz;

    auto operator==(const block_key& other) const -> bool = default;
};

struct chunk_key_hash {
    auto operator()(const chunk_key& key) const noexcept -> std::size_t
    {
        const std::size_t hx = std::hash<int>{}(key.cx);
        const std::size_t hz = std::hash<int>{}(key.cz);
        return hx ^ (hz << 1);
    }
};

struct block_key_hash {
    auto operator()(const block_key& key) const noexcept -> std::size_t
    {
        const std::size_t hx = std::hash<int>{}(key.bx);
        const std::size_t hy = std::hash<int>{}(key.by);
        const std::size_t hz = std::hash<int>{}(key.bz);
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

inline constexpr glz::opts k_json_read_opts{.null_terminated = false, .error_on_unknown_keys = false};
inline constexpr glz::opts k_json_write_opts{.prettify = true};

} // namespace persistence_json
