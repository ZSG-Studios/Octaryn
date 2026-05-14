# Gameplay Migration Map

| Old source | Old surface | New owner | Required bridge |
| --- | --- | --- | --- |
| `references/old-architecture/source/app/player/physics/movement.cpp` | `player_move` | basegame high-level movement intent/rules plus server authoritative physics execution | frame action snapshot, server physics command/result contract, client prediction copy |
| `references/old-architecture/source/app/player/physics/physics.cpp` | `player_move_walk` | basegame movement rules plus server authoritative physics execution | character movement intent command, server physics command/result contract, client prediction copy |
| `references/old-architecture/source/app/player/physics/physics.cpp` | `player_move_fly` | basegame movement rules plus server authoritative physics execution | camera movement intent command, server physics command/result contract, client prediction copy |
| `references/old-architecture/source/app/player/player.cpp` | `player_toggle_controller` | `octaryn-basegame/Source/Gameplay/Player/PlayerControlState.cs` | set player control mode command |
| `references/old-architecture/source/app/player/player.cpp` | `player_rotate` | `octaryn-basegame/Source/Gameplay/Actions/PlayerActionIntentSystem.cs` | mouse delta snapshot |
| `references/old-architecture/source/app/player/player.cpp` | `player_update_query` | `octaryn-basegame/Source/Gameplay/Interaction/BlockInteractionSystem.cs` | raycast hit snapshot |
| `references/old-architecture/source/app/player/blocks.cpp` | `player_place_block` | `octaryn-basegame/Source/Gameplay/Interaction/BlockInteractionSystem.cs` | host module command request facade to server authority |
| `references/old-architecture/source/app/player/blocks.cpp` | `player_break_block` | `octaryn-basegame/Source/Gameplay/Interaction/BlockInteractionSystem.cs` | host module command request facade to server authority |
| `references/old-architecture/source/app/player/blocks.cpp` | `player_select_block` | `octaryn-basegame/Source/Gameplay/Player/PlayerInventorySystem.cs` | target block snapshot |
| `references/old-architecture/source/app/player/blocks.cpp` | `player_change_block` | `octaryn-basegame/Source/Gameplay/Player/PlayerInventorySystem.cs` | input edge snapshot |
| `references/old-architecture/source/app/player/spawn.cpp` | `player_save_or_load` | `octaryn-basegame/Source/Gameplay/Entities/PlayerPersistenceRules.cs` | host player persistence snapshot contract |
| `references/old-architecture/source/app/runtime/events.cpp` | gameplay input actions | `octaryn-basegame/Source/Gameplay/Actions/PlayerActionIntentSystem.cs` | input edge snapshot |
| `references/old-architecture/source/world/runtime/query.cpp` | `world_raycast` | server/shared query contract | raycast snapshot contract |
| `references/old-architecture/source/world/edit/queue.cpp` | `world_queue_block_edit` | server authority API | block edit request/result contract |
| `references/old-architecture/source/world/edit/apply.cpp` | `world_try_apply_block_edit` | server authority API | block edit validation/result contract |
