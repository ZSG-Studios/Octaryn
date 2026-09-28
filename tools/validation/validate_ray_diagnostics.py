#!/usr/bin/env python3
"""Check actual-query diagnostics; never interpret these instrumented runs as FPS evidence."""
import argparse
import csv
import json
from pathlib import Path


def validate(path: Path, reference: bool = False, require_secondary: bool = False, expected_wave: int = 0) -> dict:
    if expected_wave not in (0, 32, 64):
        raise ValueError('Expected reflection wave must be 0, 32 or 64')
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("Ray diagnostics contain no completed frames")
    frames = set()
    totals = {}
    wave_min, wave_max, wave_samples = None, None, 0
    for index, row in enumerate(rows):
        values = {key: int(value) for key, value in row.items()}
        frame = values["frame"]
        if frame in frames or values["schema_version"] not in (1, 2, 3, 4, 5, 6, 7):
            raise ValueError(f"Invalid/duplicate frame or schema at row {index}")
        frames.add(frame)
        if any(value < 0 for value in values.values()):
            raise ValueError(f"Negative counter at frame {frame}")
        receivers = values["shadow_receivers"]
        accepted = values["shadow_history_accepted"]
        rejected = values["shadow_history_rejected"]
        if not reference and accepted + rejected != receivers:
            raise ValueError(f"Shadow history partition mismatch at frame {frame}")
        if values["shadow_queries"] > receivers * (8 if reference else 4):
            raise ValueError(f"Shadow query budget exceeded at frame {frame}")
        if values["reflection_history_rejected"] > values["reflection_receivers"]:
            raise ValueError(f"Reflection history partition mismatch at frame {frame}")
        # Forward shading can also reflect; its queries intentionally contribute
        # to the total without counting as temporal reflection receivers.
        screen_hits = values.get("reflection_screen_hits", 0)
        if values["schema_version"] >= 3:
            if values["reflection_screen_attempts"] != screen_hits + values["reflection_screen_fallbacks"]:
                raise ValueError(f"Screen reflection partition mismatch at frame {frame}")
        if values["reflection_visibility_queries"] > values["reflection_queries"] + screen_hits:
            raise ValueError(f"Secondary visibility exceeds primary reflection queries at frame {frame}")
        if values["schema_version"] >= 4:
            if not (values["reflection_emission_only"] <= values["reflection_material_skipped"] <=
                    values["reflection_unlit_hits"] <= values["reflection_shaded_hits"] <=
                    values["reflection_queries"] + screen_hits):
                raise ValueError(f"Deferred material partition mismatch at frame {frame}")
        if values['schema_version'] >= 7:
            cells, rays, reconstructed, full, published = (values['reflection_quad_' + name]
                for name in ('cells', 'source_rays', 'reconstructed', 'full_rate', 'published'))
            if rays > cells or published > cells or rays > values['reflection_queries'] or reconstructed + full > values['reflection_receivers']:
                raise ValueError(f'Rough-quad work partition mismatch at frame {frame}')
            if cells and reconstructed + full != values['reflection_receivers']:
                raise ValueError(f'Rough-quad receiver coverage incomplete at frame {frame}')
        if values['schema_version'] >= 6:
            logical, tests, saved, actual = (values['reflection_witness_' + name]
                                             for name in ('logical', 'tests', 'saved', 'queries'))
            if logical != saved + actual or not (saved <= tests <= logical) or actual > values['reflection_visibility_queries']:
                raise ValueError(f'Visibility witness query partition mismatch at frame {frame}')
        if values['schema_version'] >= 5:
            keys = ('reflection_wave_observations', 'reflection_wave_min', 'reflection_wave_max')
            if any(key not in values for key in keys):
                raise ValueError(f'Missing wave telemetry at frame {frame}')
            samples, low, high = (values[key] for key in keys)
            if (not samples and (low or high)) or (samples and (low not in (4, 8, 16, 32, 64, 128) or
                    high not in (4, 8, 16, 32, 64, 128) or low > high)):
                raise ValueError(f'Invalid wave telemetry at frame {frame}')
            if samples:
                if expected_wave and (low != expected_wave or high != expected_wave):
                    raise ValueError(f'Observed reflection wave differs from requested width at frame {frame}')
                wave_samples += samples
                wave_min = min(wave_min, low) if wave_min is not None else low
                wave_max = max(wave_max, high) if wave_max is not None else high
        for key, value in values.items():
            if key not in ("frame", "schema_version", "dynamic_instance_start", "max_reflection_instance",
                           "reflection_wave_min", "reflection_wave_max"):
                totals[key] = totals.get(key, 0) + value
    if not totals["shadow_queries"] or not (totals["reflection_queries"] + totals.get("reflection_screen_hits", 0)):
        raise ValueError("Capture did not exercise both shadow and reflection queries")
    if require_secondary and not totals["reflection_visibility_queries"]:
        raise ValueError("Expected reflective geometry produced no secondary-hit visibility queries")
    if expected_wave and not wave_samples:
        raise ValueError('Requested wave width has no GPU observations')
    return {"frames": len(rows), "reference": reference, "timing_qualification": False, "totals": totals,
            "rough_quad": dict(exercised=totals.get('reflection_quad_reconstructed', 0)>0,
                               reconstructed=totals.get('reflection_quad_reconstructed', 0),
                               full_rate=totals.get('reflection_quad_full_rate', 0),
                               cells=totals.get('reflection_quad_cells', 0)),
            "reflection_wave": dict(observations=wave_samples, minimum=wave_min, maximum=wave_max,
                                    status='observed' if wave_samples else 'unknown')}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--reference", action="store_true")
    parser.add_argument("--require-secondary", action="store_true")
    parser.add_argument("--expected-wave", type=int, choices=(0, 32, 64), default=0)
    args = parser.parse_args()
    print(json.dumps(validate(args.csv, args.reference, args.require_secondary, args.expected_wave), indent=2))


if __name__ == "__main__":
    main()
