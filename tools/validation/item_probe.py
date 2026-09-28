"""Drop one apple via the mailbox, walk over it, check exact drop/pickup events."""
import time

from item_probe_support import ItemProbe, check_single_pickup


def main():
    with ItemProbe("item-vacuum") as probe:
        pose = probe.wait_pose()
        print(f"PROBE spawn=({pose['playerX']:.2f},{pose['playerY']:.2f},{pose['playerZ']:.2f})")
        probe.write("ui_action.json", {"version": 1, "seq": 1, "actions": ["inventory.drop"]})
        frame = ack = 0
        start = time.monotonic()
        while time.monotonic() - start < 5.0:
            frame += 1
            pose = probe.pose()
            if pose:
                ack = max(ack, pose.get("acknowledgedInputFrame", 0))
            probe.input(frame, -0.25, 0.6, ack, forward=1.0)
            time.sleep(1.0 / 60.0)
        time.sleep(1.0)
    if probe.server.returncode != 0:
        raise RuntimeError(f"server_exit={probe.server.returncode}")
    check_single_pickup(probe.text())
    print("PROBE=passed")


if __name__ == "__main__":
    main()

