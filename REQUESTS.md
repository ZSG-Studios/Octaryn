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

