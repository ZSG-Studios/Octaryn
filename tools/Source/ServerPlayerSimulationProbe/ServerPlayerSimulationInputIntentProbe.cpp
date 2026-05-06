#include "PlayerSimulation.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_close(std::string_view label, float actual, float expected,
                  float epsilon = 0.001f) {
  if (std::fabs(actual - expected) <= epsilon) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch actual=%f expected=%f\n",
               static_cast<int>(label.size()), label.data(), actual, expected);
  return false;
}

} // namespace

bool validate_input_intent_file() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_player_input_intent_probe.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  output << "{\"version\":1,\"frameIndex\":12,\"deltaSeconds\":0.05,"
            "\"flags\":1,\"controller\":1,\"moveX\":0.25,\"moveY\":0.5,"
            "\"moveZ\":0.75,\"cameraX\":1.0,\"cameraY\":2.0,"
            "\"cameraZ\":3.0,\"cameraPitch\":0.1,\"cameraYaw\":0.2,"
            "\"relativeMouse\":1}\n";
  output.close();

  const std::string output_path_text = output_path.string();
  OctarynServerPlayerInputIntent intent{};
  bool ok = true;
  ok &= expect_true("input intent file read",
                    octaryn_server_player_read_input_intent_file(
                        output_path_text.c_str(), &intent) == 0);
  ok &= expect_true("input intent file frame", intent.frame_index == 12u);
  ok &= expect_close("input intent file delta",
                     static_cast<float>(intent.delta_seconds), 0.05f);
  ok &= expect_true("input intent file flags", intent.input.flags == 1u);
  ok &= expect_close("input intent file move z", intent.input.move_z, 0.75f);
  ok &= expect_true("input intent file relative mouse",
                    intent.input.relative_mouse == 1);
  OctarynServerPlayerInputProcessPlan plan{};
  ok &= expect_true("input intent process plan",
                    octaryn_server_player_plan_input_intent(0, 0u, &intent,
                                                            &plan) == 0);
  ok &= expect_true("input intent process ticks", plan.should_tick == 1u);

  output.open(output_path, std::ios::binary | std::ios::trunc);
  output << "{\"version\":1,\"frameIndex\":0,\"deltaSeconds\":0.05}\n";
  output.close();
  ok &= expect_true("input intent file rejects unsupported",
                    octaryn_server_player_read_input_intent_file(
                        output_path_text.c_str(), &intent) == -4);
  ok &= expect_true("input intent unsupported plan",
                    octaryn_server_player_plan_input_intent(-4, 0u, &intent,
                                                            &plan) == 0 &&
                        plan.reason == 4u && plan.should_continue == 0u);
  ok &= expect_true(
      "input intent unsupported reason name",
      std::string_view{octaryn_server_player_input_process_reason_name(
          plan.reason)} == std::string_view{"unsupported_intent"});

  ok &= expect_true("input intent missing plan",
                    octaryn_server_player_plan_input_intent(1, 1u, &intent,
                                                            &plan) == 0 &&
                        plan.reason == 1u && plan.should_continue == 1u);
  ok &= expect_true(
      "input intent missing reason name",
      std::string_view{octaryn_server_player_input_process_reason_name(
          plan.reason)} == std::string_view{"waiting_for_intent"});

  std::filesystem::remove(output_path, error);
  return ok;
}
