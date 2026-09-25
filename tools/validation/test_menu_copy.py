"""Keep product menus concise and free of startup/developer filler copy."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RML = ROOT / "octaryn-basegame/Assets/Ui/game.rml"
RCSS = ROOT / "octaryn-basegame/Assets/Ui/game.rcss"
STARTUP = ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldStartupSurface.cpp"
UPDATE = ROOT / "octaryn-client/Source/Ui/GameUi/GameUiUpdate.cpp"


class MenuCopy(unittest.TestCase):
    def test_main_menu_is_action_first(self):
        source = RML.read_text(encoding="utf-8")
        self.assertIn('<div class="heading"><h1>Play</h1></div>', source)
        for phrase in ("Welcome back, builder", "Choose your adventure",
                       "Explore your worlds", "local authoritative server"):
            self.assertNotIn(phrase, source)

    def test_loading_surface_has_only_actionable_status(self):
        source = RML.read_text(encoding="utf-8")
        startup = STARTUP.read_text(encoding="utf-8")
        for phrase in ("loading-tip", "window stays responsive", "You can move",
                       "resize or close this window"):
            self.assertNotIn(phrase, source + startup)
        self.assertIn('id="loading-status"', source)
        self.assertIn("id='stage'", startup)

    def test_default_status_does_not_fill_empty_menu(self):
        self.assertIn('const char* statuses[]={"",', UPDATE.read_text(encoding="utf-8"))
        self.assertIn(".status:empty", RCSS.read_text(encoding="utf-8"))

    def test_primary_actions_remain(self):
        source = RML.read_text(encoding="utf-8")
        for label in ("Singleplayer", "Multiplayer", "Settings", "Exit game"):
            self.assertIn(label, source)


if __name__ == "__main__":
    unittest.main()
