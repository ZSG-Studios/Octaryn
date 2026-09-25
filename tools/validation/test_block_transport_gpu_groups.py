"""CPU tests for strict grouped GPU evidence handling; never launches a process."""
import contextlib
import argparse
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import run_block_transport_gpu as runner
from block_transport_gpu_groups import GROUPS, inspect_group, inspect_group_inventory, inspect_pacing
from block_transport_gpu_provenance import shader_snapshot, verify_shader_snapshot


def log_for(name, frames=2):
    pipelines, rays, masks, proofs = GROUPS[name]
    if name == 'numerical':
        frames = 58
    lines = [f'block_transport_setup=passed group={name} pipelines={pipelines} ray_bounds={rays} plant_masks={masks} before_frames=1']
    for proof in proofs:
        suffix = ' first_frame=2 oracle_frames=56' if proof == 'probe' else ''
        lines.append(f'block_transport_{proof}=passed hardware=1 validation_errors=0{suffix}')
    lines.append(f'block_transport_pipeline_reuse=passed group={name} pipelines={pipelines} uses={pipelines} runtime_compiles=0')
    lines.append(f'block_transport_group=passed name={name} frames={frames} validation_errors=0')
    return '\n'.join(lines) + '\n', frames


def heartbeat(path, frames, duration=34):
    path.write_text('frame,total_ms\n' + ''.join(f'{frame},{duration}\n' for frame in range(frames)))


def shader_roots(root, fixture):
    paths = (fixture / 'Client/Shaders', root / 'octaryn-client/Shaders',
             root / 'tools/Source/ClientWorldMeshProbe')
    for directory in paths:
        directory.mkdir(parents=True)
        (directory / 'Test.slang').write_text('// CPU mock only\n')
    return paths


class GroupEvidence(unittest.TestCase):
    def test_each_group_metadata_and_pacing(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame-timing.csv'
            for name in GROUPS:
                with self.subTest(group=name):
                    log, frames = log_for(name)
                    heartbeat(path, frames)
                    self.assertEqual(inspect_group(log, name), frames)
                    self.assertEqual(inspect_pacing(path, log), frames)

    def test_pipeline_missing_unused_or_runtime_compile_rejected(self):
        log, _ = log_for('cache')
        for invalid in (log.replace('pipelines=7', 'pipelines=8'),
                        log.replace('uses=7', 'uses=6'),
                        log.replace('runtime_compiles=0', 'runtime_compiles=1'),
                        log.replace('block_transport_select=passed', 'block_transport_select=missing')):
            with self.assertRaises(RuntimeError):
                inspect_group(invalid, 'cache')

    def test_duplicate_group_and_validation_failure_rejected(self):
        log, _ = log_for('cache')
        for invalid in (log + log, log + 'rhi_validation severity=error broken\n',
                        log.replace('validation_errors=0', 'validation_errors=1')):
            with self.assertRaises(RuntimeError):
                inspect_group(invalid, 'cache')

    def test_aggregate_requires_every_group_once_in_order(self):
        records = [log_for(name)[0] for name in GROUPS]
        inspect_group_inventory(''.join(records))
        for invalid in (records[:-1], records + [records[0]], list(reversed(records))):
            with self.assertRaises(RuntimeError):
                inspect_group_inventory(''.join(invalid))

    def test_no_uncapped_skipped_or_truncated_heartbeat(self):
        log, frames = log_for('cache')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame-timing.csv'
            for text in ('frame,total_ms\n0,34\n', 'frame,total_ms\n0,34\n0,34\n',
                         'frame,total_ms\n0,34\n2,34\n', 'frame,total_ms\n0,34\n1,20\n',
                         'frame,total_ms\n0,34\n1,nan\n'):
                path.write_text(text)
                with self.assertRaises(RuntimeError):
                    inspect_pacing(path, log, 'cache')
            heartbeat(path, frames)
            self.assertEqual(inspect_pacing(path, log, 'cache'), frames)

    def test_oracle_suffix_cannot_omit_earlier_work(self):
        log, frames = log_for('numerical')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame-timing.csv'
            heartbeat(path, frames)
            for invalid in (log.replace('first_frame=2', 'first_frame=0'),
                            log.replace('oracle_frames=56', 'oracle_frames=55')):
                with self.assertRaises(RuntimeError):
                    inspect_pacing(path, invalid, 'numerical')

    def test_process_failure_preserves_artifacts_and_runs_remaining_groups(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / 'fixture'
            fixture.mkdir()
            (fixture / 'octaryn_client_world_mesh_probe.exe').write_bytes(b'CPU mock only')
            shader_roots(root, fixture)
            evidence = root / 'evidence'
            evidence.mkdir()
            calls = []

            def capture(command, cwd, env, log, *, timeout):
                name = command[-1]
                calls.append((name, timeout, command[1:-1], cwd))
                text, frames = log_for(name)
                log.write(text.encode())
                heartbeat(cwd / 'frame-timing.csv', frames)
                return 7 if name == 'cache' else 0

            with patch.object(runner, 'run_capture', side_effect=capture), patch.object(runner, 'inspect_log') as aggregate, patch.object(runner, 'inspect_group_proofs', side_effect=inspect_group):
                with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(RuntimeError):
                    runner.run_groups(root, fixture, 'd3d12', 30, evidence)
                aggregate.assert_not_called()
            self.assertEqual([call[0] for call in calls], list(GROUPS))
            self.assertTrue(all(call[1:3] == (30, ['--block-transport-only', '--group']) for call in calls))
            result = json.loads((evidence / 'result.json').read_text())
            self.assertEqual(result['status'], 'failed')
            self.assertEqual(result['shaders_sha256'], json.loads((evidence / 'shaders.json').read_text())['sha256'])
            self.assertEqual(result['groups'][0]['exit_code'], 7)
            self.assertTrue(all(group['status'] == 'passed' for group in result['groups'][1:]))
            for name in GROUPS:
                self.assertTrue((evidence / name / 'stdout.log').is_file())
                self.assertTrue((evidence / name / 'frame-timing.csv').is_file())
                self.assertTrue((evidence / name / 'result.json').is_file())

    def test_shader_snapshot_guards_content_additions_and_removal_in_all_roots(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / 'fixture'
            paths = shader_roots(root, fixture)
            expected = shader_snapshot(root, fixture)
            (root / 'unrelated.slang').write_text('// outside the actual shader directories\n')
            verify_shader_snapshot(root, fixture, expected)
            for path in paths:
                source = path / 'Test.slang'
                original = source.read_bytes()
                source.write_bytes(original + b'// mutation\n')
                with self.assertRaises(RuntimeError):
                    verify_shader_snapshot(root, fixture, expected)
                source.write_bytes(original)
                extra = path / 'Added.slang'
                extra.write_text('// added\n')
                with self.assertRaises(RuntimeError):
                    verify_shader_snapshot(root, fixture, expected)
                extra.unlink()
                source.unlink()
                with self.assertRaises(RuntimeError):
                    verify_shader_snapshot(root, fixture, expected)
                source.write_bytes(original)
            verify_shader_snapshot(root, fixture, expected)

    def test_changed_shader_blocks_later_processes_and_aggregate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / 'fixture'
            paths = shader_roots(root, fixture)
            (fixture / 'octaryn_client_world_mesh_probe.exe').write_bytes(b'CPU mock only')
            evidence = root / 'evidence'
            evidence.mkdir()

            def capture(command, cwd, env, log, *, timeout):
                text, frames = log_for(command[-1])
                log.write(text.encode())
                heartbeat(cwd / 'frame-timing.csv', frames)
                (paths[2] / 'Test.slang').write_text('// changed during the process\n')
                return 0

            with patch.object(runner, 'run_capture', side_effect=capture) as run, patch.object(runner, 'inspect_log') as aggregate:
                with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(RuntimeError):
                    runner.run_groups(root, fixture, 'd3d12', 30, evidence)
                self.assertEqual(run.call_count, 1)
                aggregate.assert_not_called()
            result = json.loads((evidence / 'result.json').read_text())
            self.assertTrue(all(group['status'] == 'failed' for group in result['groups']))
            self.assertTrue(all('Shader sources changed' in group['error'] for group in result['groups']))

    def test_timeout_ceiling_remains_thirty_seconds(self):
        self.assertEqual(runner.bounded_timeout('30'), 30)
        for value in ('0', '31', '300'):
            with self.assertRaises(argparse.ArgumentTypeError):
                runner.bounded_timeout(value)

    def test_subset_never_claims_full_suite_or_skips_strict_group_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / 'fixture'
            shader_roots(root, fixture)
            (fixture / 'octaryn_client_world_mesh_probe.exe').write_bytes(b'CPU mock only')
            evidence = root / 'evidence'
            evidence.mkdir()
            calls = []

            def capture(command, cwd, env, log, *, timeout):
                calls.append(command[-1])
                text, frames = log_for(command[-1])
                log.write(text.encode())
                heartbeat(cwd / 'frame-timing.csv', frames)
                return 0

            with patch.object(runner, 'run_capture', side_effect=capture), patch.object(runner, 'inspect_log') as aggregate, patch.object(runner, 'inspect_group_proofs', side_effect=inspect_group) as proofs:
                with contextlib.redirect_stdout(io.StringIO()):
                    result = runner.run_groups(root, fixture, 'd3d12', 30, evidence, ['convergence', 'sampling'])
                aggregate.assert_not_called()
                self.assertEqual(proofs.call_count, 2)
            self.assertEqual(calls, ['sampling', 'convergence'])
            self.assertEqual(result['scope'], 'selected_groups')
            self.assertFalse(result['full_suite'])
            self.assertEqual(result['oracle_frames'], 0)
            self.assertEqual(result['expected_groups'], calls)
            self.assertFalse((root / 'logs/tools/block-transport-d3d12.log').exists())
            self.assertTrue((root / 'logs/tools/block-transport-d3d12-selected.log').exists())

    def test_selected_sampling_keeps_numerical_thresholds(self):
        text, _ = log_for('sampling')
        proof = ('production_sampling=1 area_integral=1 cosine_moments=1 dyadic_coverage=1 '
                 'joint_dimensions=1 batch_carry=1 sequence_readback=1 mapped_rounding_audit=1 '
                 'samples=24582 worst_moment=.001 relative_area_error=.001 ')
        text = text.replace('block_transport_sampling=passed ', 'block_transport_sampling=passed ' + proof)
        runner.inspect_group_proofs(text, 'sampling')
        for invalid in (text.replace('relative_area_error=.001', 'relative_area_error=.004'),
                        text.replace('worst_moment=.001', 'worst_moment=nan'),
                        text.replace('samples=24582', 'samples=2'),
                        text.replace('joint_dimensions=1', 'joint_dimensions=0'),
                        text.replace('sequence_readback=1', 'sequence_readback=0'),
                        text.replace('mapped_rounding_audit=1', 'mapped_rounding_audit=0')):
            with self.assertRaises(RuntimeError):
                runner.inspect_group_proofs(invalid, 'sampling')

    def test_selected_groups_require_unique_known_names(self):
        self.assertEqual(runner.select_groups(list(reversed(GROUPS))), list(GROUPS))
        for names in ([], ['sampling', 'sampling'], ['bogus']):
            with self.assertRaises(ValueError):
                runner.select_groups(names)

    def test_selected_convergence_keeps_every_observation_and_error_gate(self):
        text, _ = log_for('convergence')
        proof = ('production_trace=1 production_direct=1 production_bounce=1 production_selection=1 '
                 'independent_geometry=1 linear_reference=1 no_missing_rows=1 rows=96 batches=64 '
                 'mean_error=.001 rms_error=.001 ')
        reference = ('independent_halton=1 double_precision=1 complete_room=1 rows=96 '
                     'rays_per_row=16384 direct_grid=128 reference_refinement_rms=.001 ')
        text = text.replace('block_transport_convergence=passed ', 'block_transport_convergence=passed ' + proof)
        text = text.replace('block_transport_reference=passed ', 'block_transport_reference=passed ' + reference)
        for batch in (1, 4, 16, 64):
            text += (f'block_transport_convergence_batch batch={batch} direct_mean_error=.001 '
                     'direct_rms_error=.001 indirect_mean_error=.001 indirect_rms_error=.001 indirect_peak_error=.001\n')
        runner.inspect_group_proofs(text, 'convergence')
        for invalid in (text.replace('direct_mean_error=.001', 'direct_mean_error=.01'),
                        text.replace('indirect_peak_error=.001', 'indirect_peak_error=.06'),
                        text.replace('batch=16', 'batch=4'),
                        text.replace('reference_refinement_rms=.001', 'reference_refinement_rms=nan')):
            with self.assertRaises(RuntimeError):
                runner.inspect_group_proofs(invalid, 'convergence')


if __name__ == '__main__':
    unittest.main()
