#pragma once

#include <cstdint>

namespace octaryn::server::world::time {

struct Date {
  int year;
  int month;
  int day;
};

struct ClockConfig {
  double real_seconds_per_day;
  int start_year;
  int start_month;
  int start_day;
  double start_seconds_of_day;
};

struct ClockState {
  ClockConfig config;
  std::uint64_t day_index;
  double seconds_of_day;
};

struct ClockSnapshot {
  Date date;
  std::uint64_t day_index;
  std::uint32_t second_of_day;
  std::uint32_t hour;
  std::uint32_t minute;
  std::uint32_t second;
  double total_world_seconds;
  float day_fraction;
};

struct ClockBlob {
  std::uint32_t version;
  std::uint64_t day_index;
  double seconds_of_day;
};

inline constexpr double WorldSecondsPerDay = 24.0 * 60.0 * 60.0;
inline constexpr std::uint32_t CurrentBlobVersion = 1;

ClockConfig default_config();
ClockConfig sanitize_config(const ClockConfig *config);
void reset(ClockState &state, const ClockConfig *config = nullptr);
void advance_real_seconds(ClockState &state, double real_seconds);
ClockSnapshot snapshot(const ClockState *state);
ClockBlob write_blob(const ClockState *state);
bool read_blob(ClockState &state, const ClockConfig *config,
               const ClockBlob &blob);
bool is_leap_year(int year);
int days_in_month(int year, int month);
long long days_from_civil(int year, unsigned month, unsigned day);
Date civil_from_days(long long day);

} // namespace octaryn::server::world::time
