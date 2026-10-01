"""Static contracts for the hidden map camera qualification fixture."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / 'octaryn-client/Source/App/OpenWorld/MapMotionValidation.h').read_text()
SESSION = (ROOT / 'octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp').read_text()
CAPTURE = (ROOT / 'tools/validation/capture_map_world.py').read_text()


class MapCameraMotionContracts(unittest.TestCase):
    def test_fixture_is_hidden_and_explicitly_gated(self):
        self.assertIn('hidden && (seconds > 0 || frame_limit > 0)', HEADER)
        self.assertIn('OCTARYN_CLIENT_MAP_CAMERA_MOTION', SESSION)
        self.assertIn('OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH', SESSION)

    def test_fixture_only_changes_camera(self):
        apply = HEADER.split('template<class Camera> void apply', 1)[1]
        self.assertNotIn('LocalPlayerPose', apply)
        self.assertNotIn('WorldControls', apply)
        self.assertIn('camera.x=', apply)
        self.assertIn('camera.yaw=', apply)
        self.assertIn('translation_rotation_settle_cut', HEADER)

    def test_capture_enables_motion_without_relaxing_watchdog(self):
        self.assertIn("parser.add_argument('--camera-motion'", CAPTURE)
        self.assertIn("OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES='0'", CAPTURE)
        self.assertIn("default=50.0", CAPTURE)
        self.assertIn('camera_motion_evidence', CAPTURE)


if __name__ == '__main__':
    unittest.main()
