"""Strict aggregate block-transport GPU numerical proof checks."""
import math
import re
from block_transport_gpu_groups import GROUPS, inspect_group, inspect_group_inventory


ORACLE_CASES = (
    'three_surface_chain', 'colored_cycles', 'unknown_slots_keep_denominator',
    'stale_link_geometry_epoch', 'recycled_slot_link_generation',
    'fresh_slot_poisoned_direct_history', 'fresh_slot_poisoned_untraced_row',
    'stale_surface_geometry_epoch',
    'stale_direct_keeps_environment', 'stale_environment_keeps_direct',
    'global_epoch_rejection', 'stale_bounce_epoch',
    'per_order_constant_radiance', 'source_removal_sealed_dark',
)


def inspect_log(text):
    inspect_group_inventory(text)
    _inspect_proofs(text, set(GROUPS))


def inspect_group_proofs(text, name):
    inspect_group(text, name)
    _inspect_proofs(text, {name})


def _inspect_proofs(text, groups):
    required = {'block_transport_' + proof for group in groups for proof in GROUPS[group][3]}
    if 'boundary' in groups:
        lines = re.findall(r'^block_transport_boundary=passed (.*)$', text, re.MULTILINE)
        if len(lines) != 1:
            raise RuntimeError('Missing exact boundary fixture summary')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        expected = dict(hardware='1', production_sampling='1', production_query='1', ieee_domain='1',
            signed_coordinates='1', large_32768='1', precision_fail_closed='1', outward_ulp='1',
            rounded_owner='1', near_wall='1', narrow_gap='1', empty_interval='1', legacy_interval='1',
            hostile_samples='691', ray_cases='7', validation_errors='0')
        if any(fields.get(key) != value for key, value in expected.items()):
            raise RuntimeError('Incomplete production boundary and ray interval proof')
    summaries = {
        'block_transport_admission': ('world_faces', 'gpu_surface_expansion', 'offscreen',
                                      'resolution_independent', 'no_screen_resources', 'integer_region', 'conditional_leaf_metadata'),
        'block_transport_world_room': ('production_admit', 'production_trace', 'production_direct',
                                       'production_bounce', 'offscreen_lit', 'camera_resolution_invariant',
                                       'point_light_oracle', 'sealed_dark_without_source'),
        'block_transport_cache': ('production_admission', 'signed_hash', 'overflow',
                                  'duplicate', 'stale_epoch'),
        'block_transport_select': ('production_select', 'holes', 'tombstones', 'partial_groups',
                                  'cyclic_fairness', 'ready_epochs', 'work_guard', 'slot_generation',
                                  'geometry_epochs', 'no_screen_resources'),
        'block_transport_sampling': ('production_sampling', 'area_integral', 'cosine_moments',
                                    'dyadic_coverage', 'joint_dimensions', 'batch_carry',
                                    'sequence_readback', 'mapped_rounding_audit'),
        'block_transport_convergence': ('production_trace', 'production_direct', 'production_bounce',
                                       'production_selection', 'independent_geometry', 'linear_reference', 'no_missing_rows'),
        'block_transport_geometry': ('production_cpu', 'production_vertices', 'animated_skin', 'mirrored_skin',
                                     'tangent_normals', 'cube_faces', 'sprite_plane', 'signed_positions'),
        'block_transport_dynamic_owner': ('production_prepare', 'inactive_cache', 'usable_scene', 'empty_lookup',
                                         'zero_indirect', 'local_direct', 'solar_visibility', 'bounded_queries'),
        'block_transport_item_lighting': ('production_shade', 'analytic_normal_incidence', 'lambert_pi',
                                         'metallic_diffuse_zero', 'specular_preserved', 'local_once',
                                         'side_isolation', 'source_removal'),
        'block_transport_dynamic': ('production_gather', 'exact_cache', 'source_once', 'true_sides',
                                   'moving_receivers', 'wall_isolation', 'source_removal', 'identity_reuse', 'local_receiver_identity',
                                   'geometry_epoch', 'receiver_epoch', 'finite_history', 'conservative_sky',
                                   'invalid_inputs', 'view_independent', 'no_screen_resources',
                                   'direct_sun_visibility', 'local_direct_separate', 'player_triangle_occlusion', 'player_removal',
                                   'player_disocclusion', 'stationary_partial_player', 'four_sample_stream',
                                   'unaffected_actor_history', 'moved_actor_reset', 'finite_scene_sun',
                                   'independent_direct_coverage', 'sun_range_fade'),
        'block_transport_raster': ('production_mrt', 'signed_coordinates', 'greedy_unit_parity', 'owned_depth', 'rotated_slot', 'preserved_slot'),
        'block_transport_rt': ('production_trace', 'exact_block_oracle', 'roof_edits',
                               'sealed_dark', 'unknown_not_sky'),
        'block_transport_probe': ('production_bounce', 'path_oracle', 'source_removal',
                                  'denominator', 'stale_epochs'),
        'block_transport_reconstruction': ('production_lookup', 'integer_coordinates', 'constant',
                                          'bilinear', 'topology', 'freshness', 'cutout_exclusion',
                                          'edge_uv', 'exact_lookup'),
        'block_transport_reconstruction_raster': ('production_mrt', 'merged_unit_uv'),
        'block_transport_history': ('production_bounce', 'world_rows', 'alternating_graphs', 'raw_orders',
                                    'held_batches', 'count_cap', 'source_removal', 'radiance_epoch',
                                    'slot_reuse', 'geometry_reset', 'finite_history'),
        'block_transport_reflection': ('production_lookup', 'local_direct', 'sun_once',
                                       'specular_preserved', 'stale_fallback', 'backface_policy',
                                       'plant_sides', 'primary_unchanged', 'default_unchanged'),
    }
    results = {}
    for name, flags in summaries.items():
        if name not in required:
            continue
        lines = [line for line in text.splitlines() if line.startswith(f'{name}=passed ')]
        if len(lines) != 1:
            raise RuntimeError(f'Expected one GPU fixture result: {name}=passed')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        for flag in ('hardware', *flags):
            if fields.get(flag) != '1':
                raise RuntimeError(f'Missing GPU fixture proof: {name} {flag}=1')
        if fields.get('validation_errors') != '0':
            raise RuntimeError(f'Missing zero-validation result: {name}')
        results[name] = fields
    if 'cache' in groups:
        domain = re.findall(r'^block_transport_contributor_domain=passed (.*)$', text, re.MULTILINE)
        if len(domain) != 1:
            raise RuntimeError('Expected one production contributor domain GPU proof')
        fields = dict(re.findall(r'(\w+)=(\S+)', domain[0]))
        expected = dict(production_admit='1', signed_axes='3', boundary_cases='12', margin='98',
                        zero_budget='1', existing_touch='1', validation_errors='0')
        if any(fields.get(key) != value for key, value in expected.items()):
            raise RuntimeError('Incomplete signed contributor domain and zero-budget production proof')
        budget = re.findall(r'^block_transport_contributor_budget=passed (.*)$', text, re.MULTILINE)
        if len(budget) != 1:
            raise RuntimeError('Expected one contributor budget GPU proof')
        fields = dict(re.findall(r'(\w+)=(\S+)', budget[0]))
        expected = dict(concurrent='256', hard_limit='8', last_use='1', zero_budget_reuse='1', unused_eviction='1')
        if any(fields.get(key) != value for key, value in expected.items()):
            raise RuntimeError('Incomplete concurrent contributor retention and capacity proof')
        eviction = re.findall(r'^block_transport_eviction=passed (.*)$', text, re.MULTILINE)
        if len(eviction) != 1:
            raise RuntimeError('Expected one GPU fixture result: block_transport_eviction=passed')
        fields = dict(re.findall(r'(\w+)=(\S+)', eviction[0]))
        for flag in ('pressure_gate', 'tombstone_lookup', 'duplicate_after_hole', 'generation',
                     'visible_retention', 'allocation_grace', 'contributor_expiry', 'live_gauges'):
            if fields.get(flag) != '1':
                raise RuntimeError(f'Missing GPU eviction proof: {flag}=1')
        if fields.get('concurrent_duplicates_after_hole') != '256':
            raise RuntimeError('Missing GPU eviction proof: concurrent_duplicates_after_hole=256')
        safety = re.findall(r'^block_transport_admission_safety=passed (.*)$', text, re.MULTILINE)
        if len(safety) != 1:
            raise RuntimeError('Expected one GPU fixture result: block_transport_admission_safety=passed')
        fields = dict(re.findall(r'(\w+)=(\S+)', safety[0]))
        for flag in ('generation_exhaustion', 'pending_deferral', 'visible_capacity_pressure'):
            if fields.get(flag) != '1':
                raise RuntimeError(f'Missing GPU admission proof: {flag}=1')
    for name, field, expected in (
            ('block_transport_admission', 'views', 3),
            ('block_transport_cache', 'collisions', 16),
            ('block_transport_cache', 'concurrent_duplicates', 256),
            ('block_transport_raster', 'face_directions', 6),
            ('block_transport_reconstruction', 'face_directions', 6),
            ('block_transport_reconstruction_raster', 'face_directions', 6)):
        if name not in results:
            continue
        if results[name].get(field) != str(expected):
            raise RuntimeError(f'Missing GPU fixture proof: {name} {field}={expected}')
    for name, field in (('block_transport_admission', 'rows'),
                        ('block_transport_world_room', 'rows'),
                        ('block_transport_raster', 'checked_pixels'),
                        ('block_transport_probe', 'scalar_checks'),
                        ('block_transport_reflection', 'scalar_checks'),
                        ('block_transport_reconstruction', 'queries'),
                        ('block_transport_reconstruction_raster', 'checked_pixels')):
        if name not in results:
            continue
        if not results[name].get(field, '').isdigit() or int(results[name][field]) <= 0:
            raise RuntimeError(f'No GPU comparisons recorded: {name} {field}')
    for name in ('block_transport_admission', 'block_transport_world_room'):
        if name not in results:
            continue
        if int(results[name]['rows']) <= 300:
            raise RuntimeError(f'Insufficient actual world-surface coverage: {name}')
    if 'room' in groups:
        for field in ('receiver_indirect', 'receiver_direct'):
            value = float(results['block_transport_world_room'].get(field, 'nan'))
            if not math.isfinite(value) or value <= 0:
                raise RuntimeError(f'No offscreen receiver energy measured: {field}')
    if 'admission' in groups:
        budget = re.findall(r'^block_transport_world_budget=passed (.*)$', text, re.MULTILINE)
        if len(budget) != 1:
            raise RuntimeError('Expected one production world-budget fixture result')
        fields = dict(re.findall(r'(\w+)=(\S+)', budget[0]))
        expected_budget = dict(production_scheduler='1', signed_bounds='1', max_columns='9',
                               max_faces='256', no_range_wrap='1', completed_sweeps='1',
                               production_eviction='1', resident_pins='1')
        if any(fields.get(key) != value for key, value in expected_budget.items()):
            raise RuntimeError('Missing bounded world scheduler or resident pin proof')
        expansion = re.findall(r'^block_transport_world_expansion=passed (.*)$', text, re.MULTILINE)
        if len(expansion) != 2:
            raise RuntimeError('Expected both actual-mesh and integer-limit expansion fixtures')
        expansion_cases = set()
        for line in expansion:
            fields = dict(re.findall(r'(\w+)=(\S+)', line))
            name = fields.get('case')
            if name not in ('resident_mesh', 'integer_limits') or name in expansion_cases:
                raise RuntimeError('Missing or duplicated world expansion case')
            expansion_cases.add(name)
            if any(fields.get(flag) != '1' for flag in ('exact_integer_keys', 'signed_overflow', 'material_flags')):
                raise RuntimeError('Missing exact world expansion proof')
            if any(not fields.get(flag, '').isdigit() or int(fields[flag]) <= 0 for flag in ('packed_faces', 'requests')):
                raise RuntimeError('No world expansion comparisons recorded')
            if name == 'integer_limits' and (fields['packed_faces'] != '12' or fields['requests'] != '12300'):
                raise RuntimeError('Incomplete maximum-quad signed-limit coverage')
            if name == 'resident_mesh' and int(fields['requests']) <= 300:
                raise RuntimeError('Insufficient actual world-mesh expansion coverage')
    if 'numerical' in groups:
        history = results['block_transport_history']
        if history.get('frames') != '29' or history.get('scalar_checks') != '3712':
            raise RuntimeError('Incomplete bounded multi-frame world history oracle')
    if 'cache' in groups:
        selection = results['block_transport_select']
        expected_selection = dict(capacity='65536', maximum='2048', full_sweep_rows='65536',
                                  selection_frames='38', guard_cases='9')
        if any(selection.get(key) != value for key, value in expected_selection.items()):
            raise RuntimeError('Incomplete bounded occupied-row scheduler qualification')
        for field in ('selected_rows', 'scalar_checks'):
            if not selection.get(field, '').isdigit() or int(selection[field]) < 65536:
                raise RuntimeError(f'Insufficient GPU scheduler comparisons: {field}')
    if 'sampling' in groups:
        sampling = results['block_transport_sampling']
        if sampling.get('samples') != '24582':
            raise RuntimeError('Incomplete progressive surface sampling qualification')
        for field in ('worst_moment', 'relative_area_error'):
            value = float(sampling.get(field, 'nan'))
            if not math.isfinite(value) or not 0 <= value < .004:
                raise RuntimeError(f'Invalid progressive surface sampling result: {field}')
    if 'convergence' in groups:
        reference = re.findall(r'^block_transport_reference=passed (.*)$', text, re.MULTILINE)
        if len(reference) != 1:
            raise RuntimeError('Missing independent linear-radiance reference')
        fields = dict(re.findall(r'(\w+)=(\S+)', reference[0]))
        expected_reference = dict(independent_halton='1', double_precision='1', complete_room='1',
                                  rows='96', rays_per_row='16384', direct_grid='128')
        if any(fields.get(key) != value for key, value in expected_reference.items()):
            raise RuntimeError('Incomplete independent closed-room reference')
        refinement = float(fields.get('reference_refinement_rms', 'nan'))
        if not math.isfinite(refinement) or not 0 <= refinement < .01:
            raise RuntimeError('Independent reference failed its own refinement check')
        convergence = results['block_transport_convergence']
        if convergence.get('rows') != '96' or convergence.get('batches') != '64':
            raise RuntimeError('Incomplete production linear-radiance convergence sequence')
        for name, ceiling in (('mean_error', .01), ('rms_error', .025)):
            value = float(convergence.get(name, 'nan'))
            if not math.isfinite(value) or not 0 <= value < ceiling:
                raise RuntimeError(f'Invalid production convergence error: {name}')
        batches = re.findall(r'^block_transport_convergence_batch (.*)$', text, re.MULTILINE)
        if len(batches) != 4:
            raise RuntimeError('Missing early and mature convergence observations')
        for line, batch in zip(batches, (1, 4, 16, 64)):
            fields = dict(re.findall(r'(\w+)=(\S+)', line))
            if fields.get('batch') != str(batch):
                raise RuntimeError('Convergence observation order changed')
            for name, ceiling in (('direct_mean_error', .01), ('direct_rms_error', .025),
                                  ('indirect_mean_error', .01), ('indirect_rms_error', .025),
                                  ('indirect_peak_error', .06)):
                value = float(fields.get(name, 'nan'))
                if not math.isfinite(value) or value < 0 or (batch == 64 and value >= ceiling):
                    raise RuntimeError(f'Invalid convergence observation: {batch} {name}')
    if 'dynamic' in groups:
        geometry = results['block_transport_geometry']
        if any(geometry.get(key) != value for key, value in dict(
                player_cases='24', item_cases='28', scalar_checks='340').items()):
            raise RuntimeError('Incomplete production receiver geometry parity qualification')
        item = results['block_transport_item_lighting']
        if item.get('cases') != '12' or item.get('scalar_checks') != '36':
            raise RuntimeError('Incomplete analytic item lighting qualification')
        dynamic = results['block_transport_dynamic']
        if any(dynamic.get(key) != value for key, value in dict(rows='192', cases='32', ray_budget='32768').items()):
            raise RuntimeError('Incomplete bounded dynamic receiver qualification')
        if not dynamic.get('scalar_checks', '').isdigit() or int(dynamic['scalar_checks']) < 180:
            raise RuntimeError('No dynamic receiver radiance comparisons')
        dynamic_cases = re.findall(r'^block_transport_dynamic_case=(\S+) status=passed\s*$', text, re.MULTILINE)
        expected_dynamic = ('constant_incident', 'view_independent', 'separate_rooms', 'moved_across_wall',
                            'oriented_sides', 'source_removed', 'identity_reused', 'receiver_epoch_mismatch',
                            'nonfinite_history', 'local_receiver_reused', 'local_direct_separate',
                            'player_clear_baseline', 'player_triangle_blocker', 'unrelated_player_motion',
                            'player_moves_clear', 'player_moves_blocked', 'player_removed',
                            'four_sample_reference', 'four_sample_batch_1', 'four_sample_batch_2', 'four_sample_batch_3', 'four_sample_batch_4',
                            'geometry_reset', 'invalid_receivers', 'unknown_coverage',
                            'closed_sky', 'opened_sky', 'sun_side_exit_open', 'sun_side_exit_blocked',
                            'sun_distance_fade', 'sun_outside_gi_coverage', 'disabled_cache')
        if dynamic_cases != list(expected_dynamic):
            raise RuntimeError('Dynamic receiver cases are missing, duplicated, or out of order')
    for name, flags, positive in (
            ('block_transport_leaves_rt', ('png_oracle', 'exact_key_admission'),
             ('opaque_fronts', 'transparent_holes', 'unsupported_back_faces', 'glass_water_blockers')),
            ('block_transport_leaves_raster', ('production_mrt', 'opaque_non_occluding', 'signed_key'),
             ('pixels',))):
        if name not in required:
            continue
        lines = re.findall(rf'^{name}=passed (.*)$', text, re.MULTILINE)
        if len(lines) != 1:
            raise RuntimeError(f'Expected one GPU fixture result: {name}=passed')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        if any(fields.get(flag) != '1' for flag in flags):
            raise RuntimeError(f'Missing leaf fixture proof: {name}')
        if any(not fields.get(flag, '').isdigit() or int(fields[flag]) <= 0 for flag in positive):
            raise RuntimeError(f'No leaf comparisons recorded: {name}')
    leaf_proofs = {
        'block_transport_leaf_origins': (dict(hardware='1', source_png='1', production_world_admission='1',
            production_sampling='1', production_rt='1', contributor_metadata='1', equal_texel_area='1', six_faces='1',
            signed_anchor='1', conditional_reflectance='1', origins='24576', opaque_texels='705',
            holes='319', validation_errors='0'), ('worst_cosine_error',)),
        'block_transport_leaf_energy': (dict(hardware='1', production_direct='1', production_bounce='1',
            production_selection='1', source_png='1', conditional_area_integral='1', no_double_count='1',
            faces='6', samples_per_face='4096', validation_errors='0'),
            ('direct_relative_error', 'outgoing_relative_error')),
    }
    for name, (expected, metrics) in leaf_proofs.items():
        if name not in required:
            continue
        lines = re.findall(rf'^{name}=passed (.*)$', text, re.MULTILINE)
        if len(lines) != 1:
            raise RuntimeError(f'Expected one conditional leaf fixture result: {name}')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        if any(fields.get(key) != value for key, value in expected.items()):
            raise RuntimeError(f'Incomplete conditional leaf domain proof: {name}')
        for metric in metrics:
            value = float(fields.get(metric, 'nan'))
            if not math.isfinite(value) or not 0 <= value < .004:
                raise RuntimeError(f'Invalid conditional leaf error: {name} {metric}')
    plant_proofs = {
        'block_transport_plant_keys': ({'cases': '32', 'duplicate_planes': '1', 'sides': '2', 'signed_int_limits': '1'}, ()),
        'block_transport_plant_masks': ({'origins': '40960', 'source_png': '1', 'alias_pdf': '1', 'conditional_reflectance': '1', 'bounded_memory': '476064'}, ()),
        'block_transport_plants_rt': ({'production_query': '1', 'png_oracle': '1', 'planes': '2', 'sides': '2', 'signed_anchor': '1'}, ('hits', 'holes')),
        'block_transport_plants_raster': ({'production_mrt': '1', 'duplicate_submissions': '1', 'planes': '2', 'sides': '2', 'signed_key': '1', 'true_normals': '1'}, ('pixels',)),
        'block_transport_plant_shadows': ({'production_shadow': '1', 'png_oracle': '1', 'crossed_plane_occlusion': '1', 'outward_bias': '1', 'no_sun_facing_flip': '1', 'cases': '12'}, ()),
        'block_transport_plant_energy': ({'hardware': '1', 'production_bounce': '1', 'exact_lookup': '1', 'opposite_sides': '1', 'colored_paths': '1', 'path_oracle': '1', 'source_removal': '1', 'no_double_count': '1', 'cases': '3', 'scalar_checks': '180', 'validation_errors': '0'}, ()),
    }
    for name, (expected, positive) in plant_proofs.items():
        if name not in required:
            continue
        lines = re.findall(rf'^{name}=passed (.*)$', text, re.MULTILINE)
        if len(lines) != 1:
            raise RuntimeError(f'Expected one GPU fixture result: {name}=passed')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        if any(fields.get(field) != value for field, value in expected.items()):
            raise RuntimeError(f'Missing plant fixture proof: {name}')
        if any(not fields.get(field, '').isdigit() or int(fields[field]) <= 0 for field in positive):
            raise RuntimeError(f'No plant comparisons recorded: {name}')
    if 'plants' in groups:
        energy_cases = re.findall(r'^block_transport_plant_energy_case=(\S+) status=passed\s*$', text, re.MULTILINE)
        if sorted(energy_cases) != ['red_source_removed', 'separate_colored_paths', 'separate_return_cycles']:
            raise RuntimeError('Plant energy cases are missing, duplicated, or unexpected')
    if 'numerical' in groups:
        actual_cases = re.findall(r'^block_transport_oracle_case=(\S+) status=passed\s*$',
                                  text, re.MULTILINE)
        if sorted(actual_cases) != sorted(ORACLE_CASES):
            raise RuntimeError('GPU oracle cases are missing, duplicated, or unexpected')
    if 'actor' in groups:
        actor_proofs = {
            'actor_history': dict(hardware='1', production_trace='1', production_direct='1',
                production_selection='1', same_previous_rays='1', moving_actor='1', real_disocclusion='1',
                unknown_denominator='1', bounded_player_queries='1', unchanged_world_ray_budget='1',
                cases='8', validation_errors='0'),
            'actor_bounds': dict(production_helper='1', cube_faces='6', plant_sides='4', stale_generation='1',
                stale_epoch='1', parallel='1', stored_world_limit='1', nonfinite='1', certified_sky='1',
                unknown_zero='1', near_world='1', near_actor='1', cases='19'),
        }
        for name, expected in actor_proofs.items():
            lines = re.findall(rf'^block_transport_{name}=passed (.*)$', text, re.MULTILINE)
            if len(lines) != 1:
                raise RuntimeError(f'Missing exact actor fixture summary: {name}')
            fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
            if any(fields.get(key) != value for key, value in expected.items()):
                raise RuntimeError(f'Incomplete actor history proof: {name}')
        expected_cases = ('old_clear_new_random_hit', 'unchanged_old_blocked_paths', 'actor_enters_old_path',
                          'actor_leaves_old_path', 'actor_removed', 'actor_behind_old_world_endpoint',
                          'unknown_paths_do_not_reset', 'stale_target_fails_closed')
        cases = re.findall(r'^block_transport_actor_history_case=(.*)$', text, re.MULTILINE)
        if len(cases) != len(expected_cases):
            raise RuntimeError('Missing previous-ray actor history cases')
        for line, name in zip(cases, expected_cases):
            fields = dict(re.findall(r'(\w+)=(\S+)', 'name=' + line))
            queries = 0 if name in ('actor_removed', 'unknown_paths_do_not_reset') else 15 if name == 'stale_target_fails_closed' else 16
            expected = dict(name=name, status='passed', previous_queries=str(queries),
                            current_queries='0' if name == 'actor_removed' else '16')
            if any(fields.get(key) != value for key, value in expected.items()):
                raise RuntimeError(f'Invalid actor path/query oracle: {name}')
    if 'local-area' in groups:
        lines = re.findall(r'^block_transport_local_area=passed (.*)$', text, re.MULTILINE)
        if len(lines) != 1:
            raise RuntimeError('Missing joint local-area source summary')
        fields = dict(re.findall(r'(\w+)=(\S+)', lines[0]))
        expected = dict(hardware='1', production_direct='1', production_bounce='1', production_selection='1',
            conditional_png='1', mixed_sources='1', exact_box_visibility='1', one_visibility_call='1',
            immediate_reset='1', cases='12', rows_per_case='16384', validation_errors='0')
        if any(fields.get(key) != value for key, value in expected.items()):
            raise RuntimeError('Incomplete joint local-area source proof')
        for key, ceiling in (('worst_relative_error', .025), ('one_origin_variance_ratio', .3)):
            value = float(fields.get(key, 'nan'))
            if not math.isfinite(value) or not 0 <= value < ceiling:
                raise RuntimeError(f'Joint local-area error/variance gate failed: {key}')
        expected_cases = ('cube_open', 'cube_mixed', 'cube_partial', 'leaf_open', 'leaf_mixed',
            'plant_a_front', 'plant_a_back', 'plant_b_front', 'plant_b_back', 'source_replaced',
            'visibility_reset', 'source_removed')
        cases = re.findall(r'^block_transport_local_area_case (.*)$', text, re.MULTILINE)
        if len(cases) != len(expected_cases):
            raise RuntimeError('Missing joint local-area cases')
        for line, name in zip(cases, expected_cases):
            fields = dict(re.findall(r'(\w+)=(\S+)', line))
            partial = name in ('cube_partial', 'visibility_reset')
            reset = 2 if name == 'visibility_reset' else 1 if name in ('source_replaced', 'source_removed') else 0
            expected = dict(name=name, rows='16384', partial=str(int(partial)), reset=str(reset))
            if any(fields.get(key) != value for key, value in expected.items()):
                raise RuntimeError(f'Incomplete joint local-area case: {name}')
            ceiling = .004 if name in ('cube_open', 'leaf_open', 'source_replaced', 'source_removed') else .025
            error = float(fields.get('relative_error', 'nan'))
            if not math.isfinite(error) or not 0 <= error < ceiling:
                raise RuntimeError(f'Joint local-area case error gate failed: {name}')
