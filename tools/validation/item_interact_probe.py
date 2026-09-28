"""Drop, settle, and target one apple without moving into vacuum range."""
import math
import re
import time

from item_probe_support import ItemProbe, check_single_pickup


def main():
    with ItemProbe("item-interact") as probe:
        pose = probe.wait_pose()
        print(f"PROBE spawn=({pose['playerX']:.2f},{pose['playerY']:.2f},{pose['playerZ']:.2f})")
        probe.write("ui_action.json", {"version": 1, "seq": 1, "actions": ["inventory.drop"]})
        frame = ack = 0
        item = None
        start = time.monotonic()
        while time.monotonic() - start < 6.0:
            frame += 1
            pose = probe.pose() or pose
            ack = max(ack, pose.get("acknowledgedInputFrame", 0))
            probe.input(frame, -0.25, 0.6, ack)
            time.sleep(1.0 / 60.0)
            matches = re.findall(
                r"item_settle id=2 count=1 x=(-?[\d.]+) y=(-?[\d.]+) z=(-?[\d.]+)",
                probe.text())
            if matches and time.monotonic() - start > 1.5:
                item = tuple(float(value) for value in matches[-1])
                break
        if item is None:
            raise RuntimeError("no_settled_item")
        if "item_pickup" in probe.text():
            raise RuntimeError("pickup_before_interact")

        dx, dy, dz = (item[0] - pose["playerX"], item[1] - pose["playerY"],
                      item[2] - pose["playerZ"])
        if math.hypot(dx, dz) < 1.8:
            raise RuntimeError("item_in_vacuum_radius")
        aim_yaw = math.atan2(dx, -dz)
        aim_pitch = math.asin(dy / math.sqrt(dx * dx + dy * dy + dz * dz))

        for interact in (False, True):
            if interact:
                if "item_pickup" in probe.text():
                    raise RuntimeError("pickup_before_interact")
                probe.write("ui_action.json", {"version": 1, "seq": 2, "actions": ["interact.use"]})
            start = time.monotonic()
            while time.monotonic() - start < 1.5:
                frame += 1
                pose = probe.pose() or pose
                ack = max(ack, pose.get("acknowledgedInputFrame", 0))
                if math.hypot(item[0] - pose["playerX"], item[2] - pose["playerZ"]) <= 1.6:
                    raise RuntimeError("player_entered_vacuum_radius")
                probe.input(frame, aim_pitch, aim_yaw, ack)
                time.sleep(1.0 / 60.0)
    if probe.server.returncode != 0:
        raise RuntimeError(f"server_exit={probe.server.returncode}")
    text = probe.text()
    check_single_pickup(text)
    targets = re.findall(r"\binteract_target_item entity=(\d+) id=2 count=1\b", text)
    collected = re.findall(r"\binteract_item entity=(\d+) id=2 collected=(?:1|True)\b", text)
    if len(collected) != 1 or collected[0] not in targets:
        raise RuntimeError(f"missing_matching_targeted_pickup targets={targets} collected={collected}")
    print("PROBE=passed")


if __name__ == "__main__":
    main()

