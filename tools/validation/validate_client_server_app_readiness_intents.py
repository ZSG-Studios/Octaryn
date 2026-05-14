#!/usr/bin/env python3
import json


def block_interaction_intent_document():
    return {
        "version": 1,
        "frameIndex": 1,
        "commands": [
            {
                "requestId": 3,
                "editX": 8,
                "editY": 32,
                "editZ": 8,
                "block": 29,
                "cameraX": 8.5,
                "cameraY": 35.0,
                "cameraZ": 8.5,
                "hitX": 8,
                "hitY": 31,
                "hitZ": 8,
            },
            {
                "requestId": 4,
                "editX": 8,
                "editY": 31,
                "editZ": 8,
                "block": 0,
                "cameraX": 8.5,
                "cameraY": 35.0,
                "cameraZ": 8.5,
                "hitX": 8,
                "hitY": 31,
                "hitZ": 8,
            },
        ],
    }


def write_chunk_view_intent(intent_path):
    intent_path.parent.mkdir(parents=True, exist_ok=True)
    intent_path.write_text(
        json.dumps(
            {
                "version": 1,
                "epoch": 1,
                "centerChunkX": 0,
                "centerChunkZ": 0,
                "radius": 1,
                "hasPreviousWindow": True,
                "previousCenterChunkX": -1,
                "previousCenterChunkZ": 0,
                "previousRadius": 1,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def write_player_input_intent(intent_path):
    intent_path.parent.mkdir(parents=True, exist_ok=True)
    intent_path.write_text(
        json.dumps(
            {
                "version": 1,
                "frameIndex": 1,
                "deltaSeconds": 1.0 / 60.0,
                "flags": 7,
                "controller": 1,
                "moveX": 1.0,
                "moveY": 1.0,
                "moveZ": 1.0,
                "cameraX": 0.0,
                "cameraY": 80.0,
                "cameraZ": 0.0,
                "cameraPitch": -0.45471975,
                "cameraYaw": 0.20943952,
                "relativeMouse": 1,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def write_block_interaction_intent(intent_path):
    intent_path.parent.mkdir(parents=True, exist_ok=True)
    intent_path.write_text(
        json.dumps(
            block_interaction_intent_document(),
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
