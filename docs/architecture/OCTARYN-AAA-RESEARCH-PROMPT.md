# Octaryn AAA Research Prompt

Use this document as the prompt for a deep research and architecture-planning pass. The goal is not to implement code. The goal is to catch every major AAA engine, game-module, modding, ECS, networking, physics, UI, world, tooling, validation, and dependency decision that Octaryn must plan before the modular port goes too far.

## Prompt To Paste Into ChatGPT

You are researching and planning the Octaryn AAA modular engine/API architecture. Treat the facts in this prompt as current project constraints. Produce a complete architecture research report and gap list. Do not invent a monolithic engine target, generic runtime bucket, or implementation shortcut that violates the owner split.

Your output must be practical for a C/C++ first voxel game platform with a managed C# gameplay API, basegame module, future games, and mods. Research the right patterns, libraries, APIs, system boundaries, data models, validation strategies, and milestones. When recommending a library, explain why it fits, where it belongs, what alternatives exist, what risks it introduces, and whether it should be exposed to modules or hidden behind an Octaryn API.

The most important outcome is a full list of systems and decisions that must be planned so nothing important is missed.


## Paste Order

Paste these focused prompt parts in order when running the research pass:

1. `docs/architecture/OCTARYN-AAA-RESEARCH-CONTEXT.md`
2. `docs/architecture/OCTARYN-AAA-RESEARCH-DEPENDENCIES.md`
3. `docs/architecture/OCTARYN-AAA-RESEARCH-SYSTEMS-A.md`
4. `docs/architecture/OCTARYN-AAA-RESEARCH-SYSTEMS-B.md`

Keep `docs/architecture/octaryn-master-plan.md`, its focused sibling files, and `docs/architecture/octaryn-appendix.md` as the source of truth if this prompt falls behind.
