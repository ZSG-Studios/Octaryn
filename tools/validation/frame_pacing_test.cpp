#include "FramePacing.h"
#include "FramePacingDisplay.h"
#include "FramePacingReport.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace octaryn::client::app;
namespace {
unsigned checks{};
void expect(bool value, const char* message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
}

int main() {
  FramePacing pacing;
  unsigned queries{};
  double display_hz = 60.0;
  const auto query = [&] { ++queries; return display_hz; };
  pacing.update_display(query);
  constexpr std::uint64_t start = 5'000'000'000ull;
  expect(pacing.remaining_ns(start, start + 6'000'000, 1, false, false) == 10'666'666,
      "6ms work at 60Hz must leave 10.67ms, not a full period");
  expect(pacing.remaining_ns(start, start + 20'000'000, 60, false, false) == 0,
      "overrun must not sleep");
  expect(pacing.remaining_ns(start, start, 0, false, false) == 0, "cap zero disables pacing");
  expect(pacing.remaining_ns(start, start, 60, false, true) == 0, "CLI benchmark bypasses cap");
  expect(pacing.remaining_ns(start, start + 6'000'000, 1, true, false) == 0,
      "display cap plus vsync must not double wait");
  expect(pacing.remaining_ns(start, start + 6'000'000, 120, true, false) == 0,
      "cap above vsync rate must not double wait");
  expect(pacing.remaining_ns(start, start + 16'666'666, 30, true, false) == 16'666'667,
      "lower explicit cap still limits with vsync");
  for (unsigned i = 0; i < 1000; ++i) pacing.update_display(query);
  expect(queries == 1 && pacing.display_queries() == 1, "steady frames must use cached display mode");

  const auto event = [&](Uint32 type, SDL_WindowID window = 7) {
    SDL_Event value{};
    value.type = type;
    value.window.windowID = window;
    if (frame_pacing_display_changed(value, 7)) pacing.invalidate_display();
  };
  event(SDL_EVENT_MOUSE_MOTION);
  event(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
  event(SDL_EVENT_WINDOW_DISPLAY_CHANGED, 8);
  pacing.update_display(query);
  expect(queries == 1, "unrelated events and other windows must not refresh the cache");
  event(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
  event(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED);
  display_hz = 144.0;
  pacing.update_display(query);
  expect(queries == 2, "display change events in a batch must coalesce into one query");
  expect(pacing.remaining_ns(start, start + 6'000'000, 1, false, false) == 944'444,
      "new display rate must change deadline");
  for (const auto type : {SDL_EVENT_WINDOW_ENTER_FULLSCREEN, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,
      SDL_EVENT_DISPLAY_ADDED, SDL_EVENT_DISPLAY_REMOVED}) {
    event(type);
    pacing.update_display(query);
  }
  expect(queries == 6, "fullscreen and display topology changes must refresh");
  display_hz = 59.94;
  event(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED);
  pacing.update_display(query);
  expect(pacing.remaining_ns(start, start, 1, false, false) == 16'683'350,
      "fractional refresh rates must not be rounded to integer FPS");
  display_hz = std::numeric_limits<double>::quiet_NaN();
  event(SDL_EVENT_DISPLAY_REMOVED);
  pacing.update_display(query);
  expect(pacing.remaining_ns(start, start, 1, false, false) == 0, "failed mode query disables display cap");
  expect(pacing.remaining_ns(start, start + 6'000'000, 60, false, false) == 10'666'666,
      "explicit cap survives missing display mode");
  for (unsigned i = 0; i < 1000; ++i) pacing.update_display(query);
  expect(queries == 8, "failed display query must not cause per-frame retries");
  expect(pacing.remaining_ns(start + 100'000'000, start + 106'000'000, 60, false, false) == 10'666'666,
      "frame after a missed deadline starts fresh without catch-up debt");
  FramePacingReport report;
  report.frame(10'666'666, 10.7f, 16.7f);
  report.report(pacing, 60, false);
  std::printf("frame_pacing_test=passed checks=%u display_queries=%u\n", checks, queries);
}
