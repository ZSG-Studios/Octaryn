# Request Buffer

`REQUESTS.md` is a repo-local buffer for extra prompts, standing requests,
cleanup directives, and temporary user instructions that should be visible to
future agents.

Agents must read this file before starting repo work. Treat entries here as
additional user instructions layered on top of `AGENTS.md`.

## Entry Types

Use one of these labels at the start of each request entry:

- `LOOP:` a standing request. Keep it in this file after acting on it.
- `ONE_TIME:` a single-use request. Remove it only after the requested work is
  fully completed and verified.

## Removal Rules

Agents may remove:

- Completed `ONE_TIME:` entries after the work is done and the final response
  reports that the entry was completed and removed.
- Duplicate entries only when the duplicate says the same thing and removing it
  does not weaken the request.
- Entries explicitly removed by the user.

Agents must not remove:

- Any `LOOP:` entry.
- Any unclear, partially completed, blocked, or unverified `ONE_TIME:` entry.
- Any request that defines architecture, ownership, naming, validation, cleanup,
  or safety policy.
- Any request that another active task still depends on.

## Editing Rules

- Do not rewrite user intent into softer language.
- Do not merge unrelated requests into one entry.
- Keep entries short, direct, and actionable.
- If a request needs clarification, leave it in place and note the blocker in
  the final response.
- If a `ONE_TIME:` entry is completed, remove only that completed entry and
  leave the rest of the file intact.

## Active Requests

- LOOP: Keep cleaning names and folders until the active project has no
  redundant owner prefixes where the path already provides ownership. In
  `octaryn-client/`, remove redundant `octaryn_client_`, `client_`, and
  `Client` prefixes from file, folder, type, and function names unless required
  for exported ABI symbols or real cross-owner contracts. In `octaryn-server/`,
  remove redundant `octaryn_server_`, `server_`, and `Server` prefixes under
  the same rules. Apply the same path-aware naming cleanup to shared/basegame
  roots where ownership is already obvious. Bring nested `Native/` and
  `Managed/` implementation folders out into clean owner-root organization when
  those nesting folders only repeat implementation language instead of behavior.
  Non-exception cleanup targets that must be completed by the loop:
  `octaryn-client/Source/Native/`, `octaryn-client/Source/Managed/`,
  `octaryn-server/Source/Native/`, and `octaryn-server/Source/Managed/` must not
  remain as top-level implementation-language buckets; move their contents into
  focused behavior/domain folders and delete those empty buckets. Remaining
  redundant `octaryn_client_*`, `octaryn_server_*`, `client_*`, `server_*`,
  `Client*`, and `Server*` names must be removed wherever the path already
  provides ownership, except for exported ABI symbols or real cross-owner
  contracts.
  Organize by focused behavior/domain, delete empty folders after moves, update
  build files and references, validate, and commit/push each coherent cleanup
  round so lost or broken work can be found.
