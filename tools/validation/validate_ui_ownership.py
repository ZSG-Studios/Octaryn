"""Check selected-game UI source ownership without launching a client."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def function_body(text, signature):
    start = text.index("{", text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[start + 1:end - 1]


def action_routes(owner, core):
    neutral = (core / "GameUi/ModuleGameUi.cpp").read_text()
    first_party = (owner / "GameUiModuleActions.cpp").read_text()
    for text, local_queue, authority_queue in (
            (neutral, "documents.poll(action)", "state_->actions"),
            (first_party, "module_documents.poll(action)", "state_->module_actions")):
        local = function_body(text, "GameUi::poll_module_screen_action(")
        authority = function_body(text, "GameUi::take_module_action(")
        if local_queue not in local or authority_queue in local:
            raise ValueError("Local declared UI poll drains the wrong queue")
        if authority_queue not in authority or "documents" in authority or "module_panel" in authority:
            raise ValueError("Authority UI drain consumes local document actions")
    host = (ROOT / "octaryn-client/Source/Host/ModuleHost.cpp").read_text()
    poll = function_body(host, "ui_poll_action(")
    if "poll_module_screen_action(action)" not in poll or "take_module_action" in poll:
        raise ValueError("host.ui polling does not use the local presentation route")
    session = (ROOT / "octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp").read_text()
    relay = function_body(session, "while (game_ui->take_module_action(action))")
    if "session.publish_ui_action(action)" not in relay or "poll_module_screen_action" in session:
        raise ValueError("Map authority relay consumes or loses the wrong action route")
    document = (core / "DeclaredScreen/DeclaredDocumentUi.cpp").read_text()
    if "queue_module_action" in document or "publish_ui_action" in document:
        raise ValueError("Declared document events are relayed as authority intents")
    print("ui_action_route_source_checks=passed client_local=1 authority_separate=1 native_event_execution=0")


def main():
    owner = ROOT / "octaryn-basegame/Source/Client/Ui"
    for name in ("GameUi.cpp", "GameUiEvents.cpp", "Inventory.cpp", "MainMenu.cpp", "ModulePanelUi.cpp"):
        if not (owner / name).is_file():
            raise ValueError(f"Basegame presentation implementation missing: {name}")
    for name in ("game.rml", "game.rcss", "inventory.rcss", "Fonts/Silkscreen-Regular.ttf"):
        if not (ROOT / "octaryn-basegame/Assets/Ui/Game" / name).is_file():
            raise ValueError(f"Basegame presentation resource missing: {name}")
    if (ROOT / "octaryn-client/Assets/Ui").exists():
        raise ValueError("Product UI assets remain in the client core")
    core = ROOT / "octaryn-client/Source/Ui"
    action_routes(owner, core)
    for path in core.rglob("*"):
        if path.suffix not in (".cpp", ".h"):
            continue
        text = path.read_text(encoding="utf-8")
        if any(marker in text for marker in ("module_panel", "Silkscreen", "Starting Octaryn", 'LoadDocument(utf8(assets/"game.rml"')):
            raise ValueError(f"Product presentation remains in core: {path.relative_to(ROOT)}")
    targets = (ROOT / "cmake/Owners/ClientTargets/ClientHostAppTargets.cmake").read_text()
    selection = targets.split("set(octaryn_selected_ui_sources)", 1)[1].split("endif()", 1)[0]
    external, first_party = selection.split("else()", 1)
    if "octaryn-basegame/" in external or "ModuleGameUi.cpp" not in external or "ModuleMenu.cpp" not in external:
        raise ValueError("External game selects first-party UI or lacks its neutral shell")
    if "octaryn-basegame/Source/Client/Ui/MainMenu.cpp" not in first_party:
        raise ValueError("Basegame menu implementation is not selected")
    for relative in re.findall(r'\$\{OCTARYN_WORKSPACE_ROOT_DIR\}/([^"\n]+)', targets):
        if not (ROOT / relative).exists():
            raise ValueError(f"UI target references missing source/include: {relative}")
    bundle = (ROOT / "cmake/Owners/ClientTargets/ClientManagedBundleTargets.cmake").read_text()
    section = bundle.split("if(NOT OCTARYN_GAME_PROJECT)", 1)[1].split("endif()", 1)[0]
    if "octaryn-basegame/Assets/Ui/Game" not in section or "octaryn-basegame/Assets/Ui/WorldLibrary" not in section:
        raise ValueError("First-party UI resources are not selected with basegame")
    for folder in (owner, core / "GameUi", core / "DeclaredScreen"):
        for path in folder.glob("*"):
            if path.suffix in (".cpp", ".h") and len(path.read_text().splitlines()) > 500:
                raise ValueError(f"UI source exceeds 500 lines: {path}")
    print("ui_ownership_source_checks=passed selected_game_shell=1 basegame_content_preserved=1 core_product_ui_assets=0")


if __name__ == "__main__":
    main()
