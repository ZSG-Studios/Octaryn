#include "Clock.h"

#include <algorithm>
#include <cmath>

namespace octaryn::server::world::time {
namespace {

double clamp_real_seconds_per_day(double value) {
  return value > 0.0 && std::isfinite(value) ? value : 1800.0;
}

double sanitize_start_seconds_of_day(double value) {
  if (!std::isfinite(value)) {
    return 12.0 * 60.0 * 60.0;
  }

  double wrapped = std::fmod(value, WorldSecondsPerDay);
  if (wrapped < 0.0) {
    wrapped += WorldSecondsPerDay;
  }
  return wrapped;
}

double sanitize_seconds_of_day(double seconds_of_day,
                               std::uint64_t &day_carry) {
  double sanitized = std::isfinite(seconds_of_day) ? seconds_of_day : 0.0;
  day_carry = 0;

  if (sanitized >= WorldSecondsPerDay) {
    const double whole_days = std::floor(sanitized / WorldSecondsPerDay);
    day_carry = static_cast<std::uint64_t>(whole_days);
    sanitized = std::fmod(sanitized, WorldSecondsPerDay);
  } else if (sanitized < 0.0) {
    sanitized = 0.0;
  }

  return sanitized;
}

} // namespace

ClockConfig default_config() {
  return ClockConfig{
      .real_seconds_per_day = 1800.0,
      .start_year = 1000,
      .start_month = 1,
      .start_day = 1,
      .start_seconds_of_day = 12.0 * 60.0 * 60.0,
  };
}

ClockConfig sanitize_config(const ClockConfig *config) {
  ClockConfig sanitized = default_config();
  if (config == nullptr) {
    return sanitized;
  }

  sanitized.real_seconds_per_day =
      clamp_real_seconds_per_day(config->real_seconds_per_day);
  sanitized.start_year = config->start_year;
  sanitized.start_month = config->start_month >= 1 && config->start_month <= 12
                              ? config->start_month
                              : sanitized.start_month;
  sanitized.start_day =
      std::clamp(config->start_day, 1,
                 days_in_month(sanitized.start_year, sanitized.start_month));
  sanitized.start_seconds_of_day =
      sanitize_start_seconds_of_day(config->start_seconds_of_day);
  return sanitized;
}

void reset(ClockState &state, const ClockConfig *config) {
  state.config = sanitize_config(config);
  state.day_index = 0;
  state.seconds_of_day = state.config.start_seconds_of_day;
}

void advance_real_seconds(ClockState &state, double real_seconds) {
  if (!std::isfinite(real_seconds) || real_seconds <= 0.0) {
    return;
  }

  const double day_scale =
      WorldSecondsPerDay /
      clamp_real_seconds_per_day(state.config.real_seconds_per_day);
  double next_seconds = state.seconds_of_day + real_seconds * day_scale;
  if (next_seconds >= WorldSecondsPerDay) {
    const double whole_days = std::floor(next_seconds / WorldSecondsPerDay);
    state.day_index += static_cast<std::uint64_t>(whole_days);
    next_seconds = std::fmod(next_seconds, WorldSecondsPerDay);
  }
  state.seconds_of_day = next_seconds;
}

ClockSnapshot snapshot(const ClockState *state) {
  ClockState fallback{};
  if (state == nullptr) {
    reset(fallback);
    state = &fallback;
  }

  const auto second_of_day =
      static_cast<std::uint64_t>(std::floor(state->seconds_of_day));
  const ClockConfig config = sanitize_config(&state->config);
  const long long start_day = days_from_civil(
      config.start_year, static_cast<unsigned>(config.start_month),
      static_cast<unsigned>(config.start_day));

  return ClockSnapshot{
      .date =
          civil_from_days(start_day + static_cast<long long>(state->day_index)),
      .day_index = state->day_index,
      .second_of_day = static_cast<std::uint32_t>(second_of_day),
      .hour = static_cast<std::uint32_t>((second_of_day / 3600u) % 24u),
      .minute = static_cast<std::uint32_t>((second_of_day / 60u) % 60u),
      .second = static_cast<std::uint32_t>(second_of_day % 60u),
      .total_world_seconds =
          static_cast<double>(state->day_index) * WorldSecondsPerDay +
          state->seconds_of_day,
      .day_fraction =
          static_cast<float>(state->seconds_of_day / WorldSecondsPerDay),
  };
}

ClockBlob write_blob(const ClockState *state) {
  if (state == nullptr) {
    return ClockBlob{
        .version = CurrentBlobVersion, .day_index = 0, .seconds_of_day = 0.0};
  }

  return ClockBlob{
      .version = CurrentBlobVersion,
      .day_index = state->day_index,
      .seconds_of_day =
          std::clamp(state->seconds_of_day, 0.0, WorldSecondsPerDay - 0.001),
  };
}

bool read_blob(ClockState &state, const ClockConfig *config,
               const ClockBlob &blob) {
  if (blob.version != CurrentBlobVersion) {
    return false;
  }

  reset(state, config);
  std::uint64_t day_carry = 0;
  state.day_index = blob.day_index;
  state.seconds_of_day =
      sanitize_seconds_of_day(blob.seconds_of_day, day_carry);
  state.day_index += day_carry;
  return true;
}

bool is_leap_year(int year) {
  if (year % 4 != 0) {
    return false;
  }
  if (year % 100 != 0) {
    return true;
  }
  return year % 400 == 0;
}

int days_in_month(int year, int month) {
  constexpr int days_per_month[] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days_per_month[month - 1];
}

long long days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned day_of_year =
      (153 * (month + (month > 2 ? -3u : 9u)) + 2) / 5 + day - 1;
  const unsigned day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return static_cast<long long>(era) * 146097LL +
         static_cast<long long>(day_of_era) - 719468LL;
}

Date civil_from_days(long long day) {
  day += 719468LL;
  const long long era = (day >= 0 ? day : day - 146096LL) / 146097LL;
  const auto day_of_era = static_cast<unsigned>(day - era * 146097LL);
  const unsigned year_of_era = (day_of_era - day_of_era / 1460 +
                                day_of_era / 36524 - day_of_era / 146096) /
                               365;
  const int year = static_cast<int>(year_of_era) + static_cast<int>(era) * 400;
  const unsigned day_of_year =
      day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const unsigned month_prime = (5 * day_of_year + 2) / 153;
  const unsigned civil_day = day_of_year - (153 * month_prime + 2) / 5 + 1;
  const unsigned civil_month =
      month_prime < 10 ? month_prime + 3 : month_prime - 9;

  return Date{
      .year = year + (civil_month <= 2 ? 1 : 0),
      .month = static_cast<int>(civil_month),
      .day = static_cast<int>(civil_day),
  };
}

} // namespace octaryn::server::world::time

namespace {

using octaryn::server::world::time::ClockBlob;
using octaryn::server::world::time::ClockConfig;
using octaryn::server::world::time::ClockSnapshot;
using octaryn::server::world::time::ClockState;
using octaryn::server::world::time::Date;

ClockState *as_clock(void *clock) { return static_cast<ClockState *>(clock); }

ClockConfig to_clock_config(const octaryn_server_world_time_config &config) {
  return ClockConfig{
      .real_seconds_per_day = config.real_seconds_per_day,
      .start_year = config.start_year,
      .start_month = config.start_month,
      .start_day = config.start_day,
      .start_seconds_of_day = config.start_seconds_of_day,
  };
}

ClockBlob to_clock_blob(const octaryn_server_world_time_blob &blob) {
  return ClockBlob{
      .version = blob.version,
      .day_index = blob.day_index,
      .seconds_of_day = blob.seconds_of_day,
  };
}

octaryn_server_world_time_date to_abi_date(const Date &date) {
  return octaryn_server_world_time_date{
      .year = date.year,
      .month = date.month,
      .day = date.day,
  };
}

octaryn_server_world_time_snapshot
to_abi_snapshot(const ClockSnapshot &snapshot) {
  return octaryn_server_world_time_snapshot{
      .date = to_abi_date(snapshot.date),
      .day_index = snapshot.day_index,
      .second_of_day = snapshot.second_of_day,
      .hour = snapshot.hour,
      .minute = snapshot.minute,
      .second = snapshot.second,
      .total_world_seconds = snapshot.total_world_seconds,
      .day_fraction = snapshot.day_fraction,
  };
}

octaryn_server_world_time_blob to_abi_blob(const ClockBlob &blob) {
  return octaryn_server_world_time_blob{
      .version = blob.version,
      .day_index = blob.day_index,
      .seconds_of_day = blob.seconds_of_day,
  };
}

} // namespace

extern "C" {

void *octaryn_server_world_time_clock_create() {
  auto *clock = new ClockState{};
  octaryn::server::world::time::reset(*clock);
  return clock;
}

void octaryn_server_world_time_clock_destroy(void *clock) {
  delete as_clock(clock);
}

void octaryn_server_world_time_clock_reset(
    void *clock, const octaryn_server_world_time_config *config) {
  auto *state = as_clock(clock);
  if (state == nullptr) {
    return;
  }

  const ClockConfig native_config =
      config == nullptr ? octaryn::server::world::time::default_config()
                        : to_clock_config(*config);
  octaryn::server::world::time::reset(*state, &native_config);
}

void octaryn_server_world_time_clock_advance(void *clock,
                                             double real_seconds) {
  auto *state = as_clock(clock);
  if (state == nullptr) {
    return;
  }

  octaryn::server::world::time::advance_real_seconds(*state, real_seconds);
}

octaryn_server_world_time_snapshot
octaryn_server_world_time_clock_snapshot(void *clock) {
  return to_abi_snapshot(
      octaryn::server::world::time::snapshot(as_clock(clock)));
}

octaryn_server_world_time_blob
octaryn_server_world_time_clock_write_blob(void *clock) {
  return to_abi_blob(octaryn::server::world::time::write_blob(as_clock(clock)));
}

uint32_t octaryn_server_world_time_clock_read_blob(
    void *clock, const octaryn_server_world_time_config *config,
    const octaryn_server_world_time_blob *blob) {
  auto *state = as_clock(clock);
  if (state == nullptr || blob == nullptr) {
    return 0u;
  }

  const ClockConfig native_config =
      config == nullptr ? octaryn::server::world::time::default_config()
                        : to_clock_config(*config);
  return octaryn::server::world::time::read_blob(*state, &native_config,
                                                 to_clock_blob(*blob))
             ? 1u
             : 0u;
}

uint64_t octaryn_server_world_time_clock_day_index(void *clock) {
  const auto *state = as_clock(clock);
  return state == nullptr ? 0u : state->day_index;
}

double octaryn_server_world_time_clock_seconds_of_day(void *clock) {
  const auto *state = as_clock(clock);
  return state == nullptr ? 0.0 : state->seconds_of_day;
}

}
