#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_WORLD_TIME_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_WORLD_TIME_API __attribute__((visibility("default")))
#endif

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
  double speed_multiplier;
  std::uint64_t tick_id;
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

struct ClockFrame {
  std::uint64_t tick_id;
  std::uint64_t day_index;
  double delta_seconds;
  double total_seconds;
};

inline constexpr double WorldSecondsPerDay = 24.0 * 60.0 * 60.0;
inline constexpr std::uint32_t CurrentBlobVersion = 1;

ClockConfig default_config();
ClockConfig sanitize_config(const ClockConfig *config);
double sanitize_speed_multiplier(double value);
void reset(ClockState &state, const ClockConfig *config = nullptr);
void set_speed_multiplier(ClockState &state, double multiplier);
void advance_real_seconds(ClockState &state, double real_seconds);
ClockFrame advance_frame(ClockState &state, double delta_seconds);
ClockSnapshot snapshot(const ClockState *state);
ClockBlob write_blob(const ClockState *state);
bool read_blob(ClockState &state, const ClockConfig *config,
               const ClockBlob &blob);
bool is_leap_year(int year);
int days_in_month(int year, int month);
long long days_from_civil(int year, unsigned month, unsigned day);
Date civil_from_days(long long day);

} // namespace octaryn::server::world::time

struct octaryn_server_world_time_date {
  int year;
  int month;
  int day;
};

struct octaryn_server_world_time_config {
  double real_seconds_per_day;
  int start_year;
  int start_month;
  int start_day;
  double start_seconds_of_day;
};

struct octaryn_server_world_time_snapshot {
  octaryn_server_world_time_date date;
  std::uint64_t day_index;
  std::uint32_t second_of_day;
  std::uint32_t hour;
  std::uint32_t minute;
  std::uint32_t second;
  double total_world_seconds;
  float day_fraction;
};

struct octaryn_server_world_time_blob {
  std::uint32_t version;
  std::uint64_t day_index;
  double seconds_of_day;
};

struct octaryn_server_world_time_frame {
  std::uint64_t tick_id;
  std::uint64_t day_index;
  double delta_seconds;
  double total_seconds;
};

struct octaryn_server_world_time_intent {
  std::int32_t version;
  std::int32_t speed_index;
  double speed_multiplier;
  std::int32_t hour_offset;
};

struct octaryn_server_world_time_intent_process_plan {
  std::uint32_t should_apply;
  std::uint32_t reason;
};

extern "C" OCTARYN_SERVER_WORLD_TIME_API void *
octaryn_server_world_time_clock_create();

extern "C" OCTARYN_SERVER_WORLD_TIME_API void
octaryn_server_world_time_clock_destroy(void *clock);

extern "C" OCTARYN_SERVER_WORLD_TIME_API void
octaryn_server_world_time_clock_reset(
    void *clock, const octaryn_server_world_time_config *config);

extern "C" OCTARYN_SERVER_WORLD_TIME_API void
octaryn_server_world_time_clock_advance(void *clock, double real_seconds);

extern "C" OCTARYN_SERVER_WORLD_TIME_API void octaryn_server_world_time_clock_set_speed_multiplier(
    void *clock, double multiplier);

extern "C" OCTARYN_SERVER_WORLD_TIME_API void
octaryn_server_world_time_clock_step_hours(void *clock, std::int32_t hours);

extern "C" OCTARYN_SERVER_WORLD_TIME_API octaryn_server_world_time_frame
octaryn_server_world_time_clock_advance_frame(void *clock, double delta_seconds);

extern "C" OCTARYN_SERVER_WORLD_TIME_API octaryn_server_world_time_snapshot
octaryn_server_world_time_clock_snapshot(void *clock);

extern "C" OCTARYN_SERVER_WORLD_TIME_API octaryn_server_world_time_blob
octaryn_server_world_time_clock_write_blob(void *clock);

extern "C" OCTARYN_SERVER_WORLD_TIME_API uint32_t
octaryn_server_world_time_clock_read_blob(
    void *clock, const octaryn_server_world_time_config *config,
    const octaryn_server_world_time_blob *blob);

extern "C" OCTARYN_SERVER_WORLD_TIME_API uint64_t
octaryn_server_world_time_clock_day_index(void *clock);

extern "C" OCTARYN_SERVER_WORLD_TIME_API double
octaryn_server_world_time_clock_seconds_of_day(void *clock);

extern "C" OCTARYN_SERVER_WORLD_TIME_API int32_t octaryn_server_world_time_read_intent_file(
    const char *intent_path, octaryn_server_world_time_intent *intent);

extern "C" OCTARYN_SERVER_WORLD_TIME_API int32_t octaryn_server_world_time_plan_intent(
    int32_t intent_read_result,
    const octaryn_server_world_time_intent *intent,
    octaryn_server_world_time_intent_process_plan *plan);

extern "C" OCTARYN_SERVER_WORLD_TIME_API const char *
octaryn_server_world_time_intent_process_reason_name(std::uint32_t reason);
