"""Source contracts for exclusive background map loading and SDL responsiveness."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "octaryn-client/Source/App/Startup/MapStartup.cpp").read_text()
APP = (ROOT / "octaryn-client/Source/App/OpenWorld/OpenWorld.cpp").read_text()


class MapStartupContracts(unittest.TestCase):
    def test_saved_temporal_settings_precede_worker_start(self):
        startup = (ROOT / "octaryn-client/Source/App/Startup/RendererStartup.cpp").read_text()
        scene = startup.split("graphics::WorldSceneSettings scene;", 1)[1]
        scene = scene.split("StartupWork::progress(\"graphics ready\"", 1)[0]
        for field in ("upscaler_mode", "fsr_sharpening", "fsr_sharpness",
                      "fsr_render_scale", "fsr_dynamic_resolution", "fsr_min_scale",
                      "fsr_max_scale", "fsr_target_fps"):
            self.assertIn(f"scene.{field}=saved.{field}", scene)
        self.assertLess(scene.index("open_world_renderer_set_scene(renderer,scene)"),
                        scene.index("open_world_renderer_prepare_temporal(renderer)"))
        self.assertIn("if(state.renderer)state.prepare_saved_settings();", startup)
        self.assertLess(APP.index("start_renderer(window, controls.running, controls.ui)"),
                        APP.index("if (!start_map("))

    def test_temporal_allocation_uses_exclusive_worker(self):
        execute = SOURCE.split("static int execute(", 1)[1].split("\n};", 1)[0]
        self.assertIn("open_world_renderer_prepare_temporal(state.renderer)", execute)
        self.assertLess(execute.index("open_world_renderer_load_map"),
                        execute.index("open_world_renderer_prepare_temporal"))
        self.assertIn("map_boot_temporal ms=", execute)
        device = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRendererDevice.cpp").read_text()
        prepare = device.split("bool open_world_renderer_prepare_temporal(", 1)[1]
        prepare = prepare.split("bool world_renderer_resize(", 1)[0]
        self.assertIn("open_world_renderer_flush(r)", prepare)
        self.assertIn("frame_queue.synchronize", prepare)
        self.assertIn("resize_targets(*r", prepare)
        self.assertNotIn("SDL_", prepare)
        self.assertNotIn("surface->", prepare)
        targets = device.split("bool resize_targets(", 1)[1].split("bool open_world_renderer_prepare_temporal", 1)[0]
        self.assertNotIn("SDL_", targets)
        self.assertNotIn("surface->", targets)

    def test_present_only_changes_preserve_temporal_resources(self):
        device = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRendererDevice.cpp").read_text()
        resize = device.split("bool world_renderer_resize(", 1)[1].split("bool world_renderer_create_device", 1)[0]
        self.assertIn("width!=r.width || height!=r.height || !r.targets[0].depth", resize)
        self.assertIn("r.temporal.requested_mode!=r.temporal.mode || r.temporal.reconfigure", resize)
        renderer = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRenderer.cpp").read_text()
        self.assertNotIn("if(mode_changed)r->temporal.mode=r->temporal.requested_mode;", renderer)

    def test_live_path_uses_background_owner(self):
        self.assertNotIn("graphics::open_world_renderer_load_map(", APP)
        self.assertIn("start_map(window, renderer,", APP)
        self.assertIn("if(!controls.running)return 0;", APP)
        build = (ROOT / "cmake/Owners/ClientTargets/ClientHostAppTargets.cmake").read_text()
        self.assertIn("/App/Startup/MapStartup.cpp", build)

    def test_worker_has_no_window_or_surface_work(self):
        execute = SOURCE.split("static int execute(", 1)[1].split("\n};", 1)[0]
        self.assertIn("open_world_renderer_load_map(state.renderer,", execute)
        self.assertIn("open_world_renderer_load_tiles(state.renderer,", execute)
        self.assertIn("catch(...) {state.failure=std::current_exception();}", execute)
        self.assertNotRegex(execute, r"SDL_(?:PollEvent|PumpEvents|WaitEvent|CreateWindow|DestroyWindow|SetWindow\w*|GetWindow\w*)")
        self.assertNotIn("surface->", execute)
        self.assertNotIn("pump_boot_stage", execute)

    def test_wait_pumps_events_without_accessing_renderer(self):
        wait = SOURCE.split("while(!octaryn_native_schedule_runtime_task_ready", 1)[1]
        wait = wait.split("octaryn_native_schedule_runtime_report report", 1)[0]
        self.assertIn("SDL_PollEvent", wait)
        self.assertIn("SDL_Delay(8)", wait)
        self.assertNotIn("graphics::", wait)
        self.assertNotIn("renderer->", wait)
        self.assertNotIn("SDL_RaiseWindow", SOURCE)
        self.assertNotIn("SDL_SetWindowRelativeMouseMode", SOURCE)

    def test_close_requests_cancellation_without_early_teardown(self):
        self.assertIn("event.type==SDL_EVENT_QUIT", SOURCE)
        self.assertIn("event.window.windowID==window_id", SOURCE)
        self.assertIn("running=false;\n        state.work.cancel();", SOURCE)
        self.assertNotIn("open_world_renderer_destroy", SOURCE)
        self.assertNotIn("detach", SOURCE)
        self.assertIn("return running && state.loaded;", SOURCE)

    def test_task_joins_before_state_or_failure_is_consumed(self):
        self.assertLess(SOURCE.index("MapStartup state;"), SOURCE.index("Task task("))
        self.assertLess(SOURCE.index("runtime_task_result(task.get(),&report)"),
                        SOURCE.index("task.reset();"))
        self.assertLess(SOURCE.index("task.reset();"),
                        SOURCE.index("if(state.failure)std::rethrow_exception"))
        self.assertIn("map_boot responsiveness=1", SOURCE)


if __name__ == "__main__":
    unittest.main()
