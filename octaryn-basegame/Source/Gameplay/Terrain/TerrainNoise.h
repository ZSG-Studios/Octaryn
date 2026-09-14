#pragma once

#include <cmath>
#include <cstdint>
#include <array>

namespace octaryn::basegame::terrain {

inline constexpr std::uint32_t Seed = 1337;
inline constexpr std::uint32_t GeneratorRevision = 2;

inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }

inline double lattice(std::int64_t x, std::int64_t y, std::int64_t z,
                      std::uint32_t channel) {
  std::uint64_t h = Seed ^ (static_cast<std::uint64_t>(channel) * 0xD6E8FEB86659FD93ull);
  h ^= static_cast<std::uint64_t>(x) * 0x9E3779B185EBCA87ull;
  h ^= static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full;
  h ^= static_cast<std::uint64_t>(z) * 0x165667B19E3779F9ull;
  h ^= h >> 30;
  h *= 0xBF58476D1CE4E5B9ull;
  h ^= h >> 27;
  h *= 0x94D049BB133111EBull;
  h ^= h >> 31;
  return static_cast<double>(h >> 11) * 0x1.0p-52 - 1.0;
}

inline double noise2(double x, double z, std::uint32_t channel) {
  const auto ix = static_cast<std::int64_t>(std::floor(x));
  const auto iz = static_cast<std::int64_t>(std::floor(z));
  const double tx = fade(x - static_cast<double>(ix));
  const double tz = fade(z - static_cast<double>(iz));
  return lerp(lerp(lattice(ix, 0, iz, channel), lattice(ix + 1, 0, iz, channel), tx),
              lerp(lattice(ix, 0, iz + 1, channel), lattice(ix + 1, 0, iz + 1, channel), tx), tz);
}

inline double noise3_plane(std::int64_t ix, std::int64_t iy, std::int64_t iz,
                           double tx, double tz, std::uint32_t channel) {
  return lerp(lerp(lattice(ix, iy, iz, channel), lattice(ix + 1, iy, iz, channel), tx),
              lerp(lattice(ix, iy, iz + 1, channel), lattice(ix + 1, iy, iz + 1, channel), tx), tz);
}

inline double noise3(double x, double y, double z, std::uint32_t channel) {
  const auto ix = static_cast<std::int64_t>(std::floor(x));
  const auto iy = static_cast<std::int64_t>(std::floor(y));
  const auto iz = static_cast<std::int64_t>(std::floor(z));
  const double tx = fade(x - static_cast<double>(ix));
  const double ty = fade(y - static_cast<double>(iy));
  const double tz = fade(z - static_cast<double>(iz));
  return lerp(noise3_plane(ix, iy, iz, tx, tz, channel),
              noise3_plane(ix, iy + 1, iz, tx, tz, channel), ty);
}

// Fixed X/Z and channel; consecutive planes occupy different slots. Full keys
// preserve arbitrary ascending, descending, skipped and repeated Y queries.
class Noise3Column {
  struct Plane {std::int64_t y{};double value{};bool valid{};};
  std::int64_t ix_,iz_;
  double tx_,tz_;
  std::uint32_t channel_;
  mutable std::array<Plane,2> planes_{};
  double plane(std::int64_t y) const {
    auto& cached=planes_[static_cast<std::uint64_t>(y)&1u];
    if(!cached.valid || cached.y!=y) {
      cached={y,noise3_plane(ix_,y,iz_,tx_,tz_,channel_),true};
    }
    return cached.value;
  }
public:
  Noise3Column(double x,double z,std::uint32_t channel)
      :ix_(static_cast<std::int64_t>(std::floor(x))),iz_(static_cast<std::int64_t>(std::floor(z))),
       tx_(fade(x-static_cast<double>(ix_))),tz_(fade(z-static_cast<double>(iz_))),channel_(channel) {}
  double sample(double y) const {
    const auto iy=static_cast<std::int64_t>(std::floor(y));
    const double ty=fade(y-static_cast<double>(iy));
    return lerp(plane(iy),plane(iy+1),ty);
  }
};

inline double fbm(double x, double z, double frequency, std::uint32_t channel,
                  int octaves = 4) {
  double total = 0, weight = 0, amplitude = 1;
  for (int octave = 0; octave < octaves; ++octave) {
    const auto octave_channel = channel + static_cast<std::uint32_t>(octave) * 101u;
    total += noise2(x * frequency, z * frequency, octave_channel) * amplitude;
    weight += amplitude;
    amplitude *= 0.5;
    frequency *= 2;
  }
  return total / weight;
}

} // namespace octaryn::basegame::terrain
