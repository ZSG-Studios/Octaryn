"""Submit isolated digging/replacement through the production authoritative command transport."""
import json
import re
import subprocess
import time


def prepare(case, bundle):
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    stone = next(i for i, block in enumerate(catalog) if block['id'].endswith('.stone'))
    path = case / 'world/world_blocks.json'
    data = json.loads(path.read_text())
    targets = {(0, 161, 3), (0, 162, 3)}
    data['blocks'] = [row for row in data['blocks'] if (row['x'], row['y'], row['z']) not in targets]
    data['blocks'] += [dict(x=x, y=y, z=z, block=stone) for x, y, z in targets]
    path.write_text(json.dumps(data), encoding='utf-8')
    return stone


def drive(process, case, stone, timeout, startup_roof=(), measure_edits=True,
          floor_y=160):
    start = time.monotonic()
    server = case / 'world/logs/server/local-session.log'
    submitted = []
    actions = [dict(action='seal_roof', target=row['target'], hit=row['hit'], block=stone, threshold=0)
               for row in startup_roof]
    if measure_edits:
        actions += [dict(action='remove', target=[0, floor_y+2, 3], hit=[0, floor_y+2, 3], block=0, threshold=8),
                    dict(action='replace', target=[0, floor_y+2, 3], hit=[0, floor_y+1, 3], block=stone, threshold=20)]
    captures = []
    seen_captures = set()
    previous_poll = 0
    while process.poll() is None:
        now = time.monotonic() - start
        if now > timeout:
            raise subprocess.TimeoutExpired(process.args, timeout)
        text = (case / 'client.log').read_text(errors='replace')
        matches = list(re.finditer(r'world_capture frame=(\d+).*?path=(.+)', text))
        for match in matches:
            frame = int(match[1])
            if frame not in seen_captures:
                seen_captures.add(frame)
                captures.append(dict(frame=frame, path=match[2].strip(), observed_seconds=now))
        log = server.read_text(errors='replace') if server.exists() else ''
        for command in submitted:
            if 'accepted_seconds' in command:
                continue
            target = ','.join(str(value) for value in command['target'])
            pattern = rf'server_live_block_command rejected=0 kind=SetBlock request={command["request"]} edit=\w+ applied=1 changed=1 block=\({target},{command["block"]}\)'
            if re.search(pattern, log):
                command['accepted_seconds'] = now
                command['acceptance_observation_interval_seconds'] = [previous_poll, now]
                command['capture_count_at_acceptance'] = len(captures)
            elif re.search(rf'server_live_client_command_rejected .*request={command["request"]} ', log):
                raise RuntimeError(f'Authoritative edit rejected: request {command["request"]}')
            elif now - command['submitted_seconds'] > 10:
                raise RuntimeError(f'Authoritative edit not accepted: request {command["request"]}')
        if captures and startup_roof and (len(submitted) < len(startup_roof) or
                any('accepted_seconds' not in row for row in submitted[:len(startup_roof)])):
            raise RuntimeError('Tunnel roof was not authoritatively sealed before initial capture')
        if len(submitted) < len(actions) and (not submitted or 'accepted_seconds' in submitted[-1]):
            action = actions[len(submitted)]
            pose_path = case / 'world/runtime/player_state.json'
            if len(matches) >= action['threshold'] and pose_path.exists():
                try:
                    pose = json.loads(pose_path.read_text())
                except (OSError, json.JSONDecodeError):
                    # The server atomically publishes this live snapshot; a
                    # Windows replacement/share race may last one poll only.
                    time.sleep(.05)
                    continue
                if startup_roof and not floor_y+2 < pose['playerY'] < floor_y+3:
                    raise RuntimeError('Authoritative player failed to spawn inside the tunnel')
                request = len(submitted) + 1
                x, y, z = action['target']
                hx, hy, hz = action['hit']
                command = dict(requestId=request, editX=x, editY=y, editZ=z, block=action['block'],
                               cameraX=pose['playerX'], cameraY=pose['playerY'], cameraZ=pose['playerZ'],
                               hitX=hx, hitY=hy, hitZ=hz)
                path = case / 'world/runtime/block_interaction.json'
                if path.exists():
                    raise RuntimeError('Prior authoritative command still pending')
                temporary = path.with_suffix('.validation.tmp')
                temporary.write_text(json.dumps(dict(version=1, frameIndex=request, commands=[command])), encoding='utf-8')
                temporary.replace(path)
                submitted.append(dict(request=request, action=action['action'], target=action['target'], block=action['block'],
                                      submitted_seconds=now, last_capture_frame=int(matches[-1][1]) if matches else 0))
        previous_poll = now
        time.sleep(.05)
    commands = [row for row in submitted if row['action'] != 'seal_roof']
    if len(commands) != (2 if measure_edits else 0) or len(submitted) != len(actions) or any('accepted_seconds' not in command for command in submitted):
        raise RuntimeError('Incomplete authoritative remove/replace sequence')
    for command in commands:
        following = [row for row in captures if row['observed_seconds'] > command['accepted_seconds']]
        if not following:
            raise RuntimeError('No GPU frame captured after authoritative edit acceptance')
        command['first_capture_after_acceptance'] = following[0]
        command['observed_capture_delay_seconds'] = following[0]['observed_seconds'] - command['accepted_seconds']
    return dict(commands=commands, startup_commands=[row for row in submitted if row['action'] == 'seal_roof'],
                captures=captures, server_log=str(server),
                scope='Acceptance and GPU capture times observed by 50ms polling. Visual response requires inspection; capture availability is not GI convergence.')
