#include "Clock.h"

#include <cmath>
#include <cstdio>
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
      .seconds_of_day = WorldSecondsPerDay * 2.0 + 12.5,
  };

  bool ok = true;
  ok &= expect_true("blob load", read_blob(clock, nullptr, blob));
  ok &= expect_equal("blob day carry", clock.day_index, 4u);
  ok &= expect_near("blob seconds", clock.seconds_of_day, 12.5);
  ok &= expect_true(
      "reject unknown blob version",
      !read_blob(
          clock, nullptr,
          ClockBlob{.version = 99, .day_index = 0, .seconds_of_day = 0.0}));
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_default_snapshot();
  ok &= validate_advance_and_date_carry();
  ok &= validate_calendar();
  ok &= validate_blob_read();

  if (!ok) {
    return 1;
  }

  std::puts("server world time native probe passed");
  return 0;
}
