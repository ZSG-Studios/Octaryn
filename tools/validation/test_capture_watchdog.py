"""CPU-only watchdog regressions; owned processes are harmless Python sleepers."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock

from capture_watchdog import FrameHealth, MapStartupHealth, ProcessTree, ShutdownFrames, run_capture


class MapStartupTests(unittest.TestCase):
    def test_cpu_loading_without_ray_stage_uses_overall_timeout(self):
        MapStartupHealth().check(100)

    def test_ray_build_deadline(self):
        health = MapStartupHealth()
        health.line('map_ray_startup_begin', 1)
        health.check(2.99)
        with self.assertRaisesRegex(RuntimeError, 'ray startup'):
            health.check(3.01)

    def test_completed_build_clears_deadline(self):
        health = MapStartupHealth()
        health.line('map_ray_startup_begin', 1)
        health.line('map_ray_startup_end elapsed_ms=1200', 2.2)
        health.check(100)

    def test_duplicate_begin_does_not_extend_deadline(self):
        health = MapStartupHealth()
        health.line('map_ray_startup_begin', 1)
        health.line('map_ray_startup_begin', 2.9)
        with self.assertRaisesRegex(RuntimeError, 'ray startup'):
            health.check(3.01)


class FrameHealthTests(unittest.TestCase):
    def test_first_frame_hitch_followed_by_recovery_is_not_sustained(self):
        health = FrameHealth(0, max_frame_ms=1000 / 15)
        health.frame(33.8241005, .034)
        health.frame(1081.53406, 1.116)
        health.frame(5.90789986, 1.122)
        self.assertEqual(health.slow_ms, 0)
        self.assertEqual(health.slow_frames, 0)

    def test_repeated_hitches_still_stop(self):
        health = FrameHealth(0, max_frame_ms=1000 / 15)
        health.frame(1081.53406, 1.082)
        with self.assertRaisesRegex(RuntimeError, 'sustained slow frames'):
            health.frame(70, 1.152)

    def test_single_hitch_does_not_disable_stall_detection(self):
        health = FrameHealth(0)
        health.frame(1081.53406, 1.082)
        with self.assertRaisesRegex(RuntimeError, 'heartbeat stalled'):
            health.check(3.083)

    def test_exact_threshold_is_healthy(self):
        health = FrameHealth(0)
        for index in range(20):
            health.frame(50, index / 20)
        self.assertEqual(health.slow_ms, 0)

    def test_sustained_slow_frames_stop_at_one_second(self):
        health = FrameHealth(0)
        for index in range(15):
            health.frame(62.5, index / 16)
        self.assertEqual(health.slow_ms, 937.5)
        with self.assertRaisesRegex(RuntimeError, 'sustained slow frames'):
            health.frame(62.5, 1)

    def test_thirty_fps_cap_is_healthy(self):
        health = FrameHealth(0)
        for index in range(60):
            health.frame(1000 / 30, index / 30)
        self.assertEqual(health.slow_ms, 0)

    def test_recovery_resets_slow_window(self):
        health = FrameHealth(0)
        for index in range(7):
            health.frame(125, index / 8)
        health.frame(16, 1)
        self.assertEqual(health.slow_ms, 0)
        for index in range(7):
            health.frame(125, 1 + index / 8)

    def test_single_long_frame_stops(self):
        with self.assertRaisesRegex(RuntimeError, 'heartbeat stalled'):
            FrameHealth(0).frame(2000, 2)

    def test_two_second_stall(self):
        health = FrameHealth(0)
        health.frame(16, 3)
        health.check(4.99)
        with self.assertRaisesRegex(RuntimeError, 'heartbeat stalled'):
            health.check(5.001)

    def test_new_frame_refreshes_heartbeat(self):
        health = FrameHealth(0)
        health.frame(16, 1)
        health.frame(16, 2.9)
        health.check(4.8)

    def test_startup_uses_overall_timeout(self):
        FrameHealth(0).check(100)


class ShutdownFrameTests(unittest.TestCase):
    def test_completed_closing_frames_use_existing_health(self):
        health = FrameHealth(0)
        health.frame(33, 0)
        shutdown = ShutdownFrames(health)
        for index in range(90):
            now = (index + 1) / 30
            shutdown.line(f'client_shutdown_frame total_ms=33.333 remaining={90 - index}', now)
            health.check(now)
        self.assertEqual(health.last_frame, 3)
        self.assertEqual(health.slow_frames, 0)

    def test_stage_messages_do_not_extend_stalled_cleanup(self):
        health = FrameHealth(0)
        health.frame(33, 0)
        shutdown = ShutdownFrames(health)
        shutdown.line('client_shutdown stage=renderer_begin', 1.9)
        with self.assertRaisesRegex(RuntimeError, 'heartbeat stalled'):
            health.check(2.001)

    def test_cleanup_slow_frames_share_world_slow_window(self):
        health = FrameHealth(0)
        health.frame(600, .6)
        shutdown = ShutdownFrames(health)
        with self.assertRaisesRegex(RuntimeError, 'sustained slow frames'):
            shutdown.line('client_shutdown_frame total_ms=400 remaining=32', 1)

    def test_sustained_slow_cleanup_is_rejected(self):
        shutdown = ShutdownFrames(FrameHealth(0))
        for index in range(7):
            shutdown.line(f'client_shutdown_frame total_ms=125 remaining={8 - index}', index / 8)
        with self.assertRaisesRegex(RuntimeError, 'sustained slow frames'):
            shutdown.line('client_shutdown_frame total_ms=125 remaining=0', 1)

    def test_completed_stalled_cleanup_frame_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'heartbeat stalled'):
            ShutdownFrames(FrameHealth(0)).line('client_shutdown_frame total_ms=2000 remaining=0', 2)

    def test_invalid_closing_durations_are_rejected(self):
        for duration in ('nan', 'inf', '-inf', '-1', 'invalid'):
            with self.subTest(duration=duration):
                with self.assertRaisesRegex(RuntimeError, 'invalid.*timing'):
                    ShutdownFrames(FrameHealth(0)).line(
                        f'client_shutdown_frame total_ms={duration} remaining=0', 1)

    def test_invalid_or_increasing_resource_counts_are_rejected(self):
        for count in ('-1', 'nan', '1.5'):
            with self.subTest(count=count):
                with self.assertRaisesRegex(RuntimeError, 'invalid shutdown frame record'):
                    ShutdownFrames(FrameHealth(0)).line(
                        f'client_shutdown_frame total_ms=33 remaining={count}', 1)
        shutdown = ShutdownFrames(FrameHealth(0))
        shutdown.line('client_shutdown_frame total_ms=33 remaining=1', 1)
        with self.assertRaisesRegex(RuntimeError, 'resource count increased'):
            shutdown.line('client_shutdown_frame total_ms=33 remaining=2', 1.1)

    def run_program(self, code, timeout=5):
        with tempfile.TemporaryDirectory(prefix='capture-watchdog-closing-') as folder:
            case = Path(folder)
            with (case / 'client.log').open('wb') as log:
                return run_capture([sys.executable, '-c', code], case, dict(os.environ), log, timeout)

    def test_capture_accepts_closing_frames_after_world_csv_stops(self):
        code = ('import pathlib,time\n'
                'pathlib.Path("frame-timing.csv").write_text("frame,total_ms\\n1,33\\n")\n'
                'for remaining in range(65,-1,-1):\n'
                ' start=time.monotonic();time.sleep(.034)\n'
                ' print(f"client_shutdown_frame total_ms={(time.monotonic()-start)*1000} "'
                'f"remaining={remaining}",flush=True)\n')
        self.assertEqual(self.run_program(code), 0)

    def test_capture_checks_final_output_after_process_exit(self):
        with self.assertRaisesRegex(RuntimeError, 'invalid frame timing'):
            self.run_program('print("client_shutdown_frame total_ms=nan remaining=0",flush=True)')

    def test_capture_preserves_nonzero_exit(self):
        self.assertEqual(self.run_program(
            'print("client_shutdown_frame total_ms=33 remaining=0",flush=True);raise SystemExit(7)'), 7)

    def test_closing_frames_do_not_extend_overall_timeout(self):
        code = ('import time\n'
                'for remaining in range(99,-1,-1):\n'
                ' time.sleep(.034)\n'
                ' print(f"client_shutdown_frame total_ms=34 remaining={remaining}",flush=True)\n')
        with self.assertRaisesRegex(RuntimeError, 'overall timeout'):
            self.run_program(code, timeout=.3)


@unittest.skipUnless(os.name == 'nt', 'Windows owned job contract')
class WindowsProcessTreeTests(unittest.TestCase):
    def test_requested_and_default_process_priority_match_native_class(self):
        code = ('import ctypes\n'
                'from ctypes import wintypes\n'
                'api=ctypes.WinDLL("kernel32",use_last_error=True)\n'
                'api.GetCurrentProcess.restype=wintypes.HANDLE\n'
                'api.GetPriorityClass.argtypes=[wintypes.HANDLE]\n'
                'api.GetPriorityClass.restype=wintypes.DWORD\n'
                'print("child_priority="+str(api.GetPriorityClass(api.GetCurrentProcess())),flush=True)')
        for requested, expected in (('normal', 0x20), ('below-normal', 0x4000), (None, 0x4000)):
            with self.subTest(requested=requested), tempfile.TemporaryDirectory(prefix='capture-priority-') as folder:
                case = Path(folder)
                with (case / 'client.log').open('wb') as log:
                    options = {} if requested is None else dict(process_priority=requested)
                    self.assertEqual(run_capture([sys.executable, '-c', code], case, dict(os.environ),
                                                 log, 5, **options), 0)
                lines = (case / 'client.log').read_text().splitlines()
                self.assertEqual(lines[0], f'capture_process_priority requested={requested or "below-normal"} '
                                          f'actual=0x{expected:08x} confirmed=1')
                self.assertEqual(lines[1], f'child_priority={expected}')

    def test_owned_wait_rechecks_unsignaled_handle(self):
        tree = ProcessTree.__new__(ProcessTree)
        tree.api = Mock()
        tree.api.WaitForSingleObject.side_effect = [258, 0]
        tree._wait_owned_processes([123], time.monotonic() + 1)
        self.assertEqual(tree.api.WaitForSingleObject.call_count, 2)
        self.assertEqual(tree.api.WaitForSingleObject.call_args_list[0].args, (123, 0))

    def test_cached_exit_code_still_waits_for_native_signal(self):
        tree = ProcessTree.__new__(ProcessTree)
        tree.job = None
        tree.process = Mock()
        tree.process._handle = 123
        tree.process.poll.return_value = 1
        tree.api = Mock()
        tree.api.WaitForSingleObject.return_value = 0
        tree.close()
        tree.api.WaitForSingleObject.assert_called_once()
        self.assertEqual(tree.api.WaitForSingleObject.call_args.args[1], 5000)
        tree.process.kill.assert_not_called()

    def test_ray_startup_is_guarded_before_frame_csv_exists(self):
        with tempfile.TemporaryDirectory(prefix='capture-watchdog-ray-') as folder:
            case = Path(folder)
            code = 'import time;print("map_ray_startup_begin",flush=True);time.sleep(60)'
            with (case / 'client.log').open('wb') as log:
                with self.assertRaisesRegex(RuntimeError, 'ray startup'):
                    run_capture([sys.executable, '-c', code], case, dict(os.environ), log, 8)

    def test_run_capture_resumes_owned_job_and_times_out(self):
        api = ctypes.WinDLL('kernel32', use_last_error=True)
        api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        api.OpenProcess.restype = wintypes.HANDLE
        api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        api.WaitForSingleObject.restype = wintypes.DWORD
        api.CloseHandle.argtypes = [wintypes.HANDLE]
        api.CloseHandle.restype = wintypes.BOOL
        with tempfile.TemporaryDirectory(prefix='capture-watchdog-run-') as folder:
            case = Path(folder)
            code = ('import pathlib,subprocess,sys,time,os;'
                    'p=subprocess.Popen([sys.executable,"-c","import time;time.sleep(60)"],'
                    'creationflags=subprocess.CREATE_NO_WINDOW);'
                    'pathlib.Path("pids").write_text(str(os.getpid())+","+str(p.pid));time.sleep(60)')
            started = time.monotonic()
            with (case / 'client.log').open('wb') as log:
                with self.assertRaisesRegex(RuntimeError, 'overall timeout'):
                    run_capture([sys.executable, '-c', code], case, dict(os.environ), log, 2)
            self.assertLess(time.monotonic() - started, 8)
            self.assertTrue((case / 'pids').exists(), 'Suspended process resumed and spawned child')
            for pid in (case / 'pids').read_text().split(','):
                handle = api.OpenProcess(0x100000, False, int(pid))
                if handle:
                    try:
                        self.assertEqual(api.WaitForSingleObject(handle, 1000), 0, 'Owned PID terminated')
                    finally:
                        api.CloseHandle(handle)
                else:
                    self.assertEqual(ctypes.get_last_error(), 87, 'PID no longer exists')

    def test_owned_parent_and_child_die_on_close(self):
        api = ctypes.WinDLL('kernel32', use_last_error=True)
        api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        api.OpenProcess.restype = wintypes.HANDLE
        api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        api.WaitForSingleObject.restype = wintypes.DWORD
        api.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int,
                                                  ctypes.c_void_p, wintypes.DWORD,
                                                  ctypes.POINTER(wintypes.DWORD)]
        api.QueryInformationJobObject.restype = wintypes.BOOL
        api.CloseHandle.argtypes = [wintypes.HANDLE]
        api.CloseHandle.restype = wintypes.BOOL
        with tempfile.TemporaryDirectory(prefix='capture-watchdog-') as folder:
            pid_path = Path(folder) / 'child.pid'
            # Gate child creation until the owned job has been assigned.
            code = ('import pathlib,subprocess,sys,time;sys.stdin.readline();'
                    'p=subprocess.Popen([sys.executable,"-c","import time;time.sleep(60)"],'
                    'creationflags=subprocess.CREATE_NO_WINDOW);'
                    'pathlib.Path(sys.argv[1]).write_text(str(p.pid));time.sleep(60)')
            process = subprocess.Popen([sys.executable, '-c', code, str(pid_path)],
                                       stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                       stderr=subprocess.DEVNULL,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            tree = None
            child_handle = None
            try:
                tree = ProcessTree(process)
                # Query the native OS layout, not a duplicate of the Python structure.
                size = 144 if ctypes.sizeof(ctypes.c_void_p) == 8 else 112
                information = ctypes.create_string_buffer(size)
                returned = wintypes.DWORD()
                self.assertTrue(api.QueryInformationJobObject(tree.job, 9, information,
                                                              size, ctypes.byref(returned)))
                self.assertEqual(returned.value, size)
                flags = int.from_bytes(information.raw[16:20], 'little')
                self.assertTrue(flags & 0x2000, 'KILL_ON_JOB_CLOSE native field')
                process.stdin.write(b'go\n')
                process.stdin.flush()
                deadline = time.monotonic() + 5
                while not pid_path.exists() and time.monotonic() < deadline:
                    time.sleep(.01)
                self.assertTrue(pid_path.exists(), 'Child published PID within five seconds')
                child_handle = api.OpenProcess(0x100000, False, int(pid_path.read_text()))
                self.assertTrue(child_handle, 'Open child synchronization handle')
                self.assertEqual(api.WaitForSingleObject(child_handle, 0), 258, 'Child initially running')
                tree.close()
                self.assertIsNotNone(process.poll(), 'Parent terminated')
                self.assertEqual(api.WaitForSingleObject(child_handle, 0), 0, 'Child terminated before close returns')
                tree.close()  # Idempotent close must not affect an unrelated process.
            finally:
                if tree:
                    tree.close()
                elif process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
                if child_handle:
                    api.CloseHandle(child_handle)
                process.stdin.close()


if __name__ == '__main__':
    unittest.main()
