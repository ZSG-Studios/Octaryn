#include "Clock.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace {

using namespace octaryn::server::world::time;

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_near(std::string_view label, double actual, double expected) {
  if (std::fabs(actual - expected) < 0.0001) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %.6f, got %.6f\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool validate_default_snapshot() {
  ClockState clock{};
  reset(clock);
  const auto snap = snapshot(&clock);

  bool ok = true;
  ok &= expect_equal("default year", snap.date.year, 1000);
  ok &= expect_equal("default month", snap.date.month, 1);
  ok &= expect_equal("default day", snap.date.day, 1);
  ok &= expect_equal("default day index", snap.day_index, 0u);
  ok &= expect_equal("default second of day", snap.second_of_day, 43200u);
  ok &= expect_equal("default hour", snap.hour, 12u);
  ok &= expect_near("default day fraction", snap.day_fraction, 0.5);
  return ok;
}

bool validate_advance_and_date_carry() {
  ClockState clock{};
  reset(clock);
  advance_real_seconds(clock, 900.0);
  const auto snap = snapshot(&clock);

  bool ok = true;
  ok &= expect_equal("advanced day index", snap.day_index, 1u);
  ok &= expect_equal("advanced year", snap.date.year, 1000);
  ok &= expect_equal("advanced month", snap.date.month, 1);
  ok &= expect_equal("advanced day", snap.date.day, 2);
  ok &= expect_equal("advanced second of day", snap.second_of_day, 0u);
  ok &= expect_equal("advanced hour", snap.hour, 0u);
  ok &= expect_near("advanced total world seconds", snap.total_world_seconds,
                    86400.0);
  return ok;
}

bool validate_frame_advance() {
  ClockState clock{};
  reset(clock);

  const auto first_frame = advance_frame(clock, 0.5);
  const auto second_frame = advance_frame(clock, -1.0);

  bool ok = true;
  ok &= expect_equal("first frame tick", first_frame.tick_id, 0u);
  ok &= expect_equal("first frame day index", first_frame.day_index, 0u);
  ok &= expect_near("first frame delta", first_frame.delta_seconds, 0.5);
  ok &= expect_near("first frame total seconds", first_frame.total_seconds,
                    43224.0);
  ok &= expect_equal("second frame tick", second_frame.tick_id, 1u);
  ok &= expect_equal("second frame day index", second_frame.day_index, 0u);
  ok &= expect_near("second frame sanitized delta",
                    second_frame.delta_seconds, 0.0);
  ok &= expect_near("second frame total seconds", second_frame.total_seconds,
                    43224.0);

  reset(clock);
  ok &= expect_equal("reset frame tick", advance_frame(clock, 0.0).tick_id,
                     0u);
  return ok;
}

bool validate_speed_multiplier() {
  ClockState clock{};
  reset(clock);
  set_speed_multiplier(clock, 2.0);
  const auto doubled_frame = advance_frame(clock, 0.5);

  bool ok = true;
  ok &= expect_near("speed multiplier stored", clock.speed_multiplier, 2.0);
  ok &= expect_near("speed multiplier delta", doubled_frame.delta_seconds,
                    1.0);
  ok &= expect_near("speed multiplier total seconds",
                    doubled_frame.total_seconds, 43248.0);

  set_speed_multiplier(clock, -4.0);
  ok &= expect_near("speed multiplier clamps low", clock.speed_multiplier,
                    0.0);
  set_speed_multiplier(clock, 24001.0);
  ok &= expect_near("speed multiplier clamps high", clock.speed_multiplier,
                    24000.0);
  set_speed_multiplier(clock, std::numeric_limits<double>::quiet_NaN());
  ok &= expect_near("speed multiplier sanitizes non-finite",
                    clock.speed_multiplier, 1.0);
  return ok;
}

bool validate_large_rollovers() {
  ClockState clock{};
  const ClockConfig config{
      .real_seconds_per_day = WorldSecondsPerDay,
      .start_year = 1000,
      .start_month = 1,
      .start_day = 1,
      .start_seconds_of_day = WorldSecondsPerDay * 3.0 + 42.5,
  };
  reset(clock, &config);

  bool ok = true;
  ok &= expect_equal("large start day index", clock.day_index, 0u);
  ok &= expect_near("large start seconds", clock.seconds_of_day, 42.5);

  advance_real_seconds(clock, WorldSecondsPerDay * 1000.0 + 12.5);
  ok &= expect_equal("large advance day index", clock.day_index, 1000u);
  ok &= expect_near("large advance seconds", clock.seconds_of_day, 55.0);

  const ClockConfig negative_config{
      .real_seconds_per_day = WorldSecondsPerDay,
      .start_year = 1000,
      .start_month = 1,
      .start_day = 1,
      .start_seconds_of_day = -10.0,
  };
  const ClockConfig sanitized_negative = sanitize_config(&negative_config);
  ok &= expect_near("negative start wraps",
                    sanitized_negative.start_seconds_of_day,
                    WorldSecondsPerDay - 10.0);
  return ok;
}

bool validate_calendar() {
  bool ok = true;
  ok &= expect_true("leap year 2000", is_leap_year(2000));
  ok &= expect_true("non-leap year 1900", !is_leap_year(1900));
  ok &= expect_equal("leap February", days_in_month(2000, 2), 29);
  ok &= expect_equal("non-leap February", days_in_month(1900, 2), 28);
  ok &= expect_equal("invalid month fallback", days_in_month(1000, 13), 31);
  return ok;
}

bool validate_blob_read() {
  ClockState clock{};
  const ClockBlob blob{
      .version = CurrentBlobVersion,
      .day_index = 2,
      .seconds_of_day = WorldSecondsPerDay * 1000.0 + 12.5,
  };

  bool ok = true;
  ok &= expect_true("blob load", read_blob(clock, nullptr, blob));
  ok &= expect_equal("blob day carry", clock.day_index, 1002u);
  ok &= expect_near("blob seconds", clock.seconds_of_day, 12.5);
  ok &= expect_true(
      "reject unknown blob version",
      !read_blob(
          clock, nullptr,
          ClockBlob{.version = 99, .day_index = 0, .seconds_of_day = 0.0}));
  return ok;
}

bool validate_world_time_intent_file() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_time_probe_intent.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  output << "{\"version\":1,\"speedIndex\":4,\"speedMultiplier\":120.5}\n";
  output.close();

  const std::string output_path_text = output_path.string();
  octaryn_server_world_time_intent intent{};
  bool ok = true;
  ok &= expect_equal("world time intent read",
                     octaryn_server_world_time_read_intent_file(
                         output_path_text.c_str(), &intent),
                     0);
  ok &= expect_equal("world time intent speed index", intent.speed_index, 4);
  ok &= expect_near("world time intent speed multiplier",
                    intent.speed_multiplier, 120.5);
  octaryn_server_world_time_intent_process_plan plan{};
  ok &= expect_equal("world time intent process plan",
                     octaryn_server_world_time_plan_intent(0, &intent, &plan),
                     0);
  ok &= expect_equal("world time intent process applies", plan.should_apply,
                     1u);

  output.open(output_path, std::ios::binary | std::ios::trunc);
  output << "{\"version\":1,\"speedMultiplier\":24001.0}\n";
  output.close();
  ok &= expect_equal("world time intent rejects unsupported",
                     octaryn_server_world_time_read_intent_file(
                         output_path_text.c_str(), &intent),
                     -4);
  ok &= expect_equal("world time intent unsupported plan",
                     octaryn_server_world_time_plan_intent(-4, &intent, &plan),
                     0);
  ok &= expect_equal("world time intent unsupported reason", plan.reason, 3u);

  std::filesystem::remove(output_path, error);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_default_snapshot();
  ok &= validate_advance_and_date_carry();
  ok &= validate_frame_advance();
  ok &= validate_speed_multiplier();
  ok &= validate_large_rollovers();
  ok &= validate_calendar();
  ok &= validate_blob_read();
  ok &= validate_world_time_intent_file();

  if (!ok) {
    return 1;
  }

  std::puts("server world time native probe passed");
  return 0;
}
