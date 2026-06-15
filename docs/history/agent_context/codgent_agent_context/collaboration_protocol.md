# Agent Collaboration Protocol

## Roles

Planner/Reviewer Agent:

- Owns architecture direction, task decomposition, and quality gates.
- Writes `taskN-plan.md` before implementation starts.
- Reviews implementation after the worker writes `taskN-summary.md`.
- Writes `taskN-review.md` with one of these decisions:
  - `APPROVED`: task is complete enough to move on.
  - `CHANGES_REQUESTED`: worker must revise the same task.
  - `BLOCKED`: task cannot proceed without user/project decision.

Implementation Agent:

- Reads the project docs and the current task plan.
- Implements only the requested scope.
- Runs the required verification commands when feasible.
- Writes `taskN-summary.md` after implementation.
- Does not silently expand architecture or change unrelated modules.

## File Naming

All cross-agent context lives in `docs/agent_context/`.

Per task:

- `taskN-plan.md`: planner instructions and acceptance criteria.
- `taskN-summary.md`: worker implementation summary and verification results.
- `taskN-review.md`: planner review, required fixes, or approval.

Support files:

- `implementation_agent_prompt.md`: reusable prompt for the worker agent.
- `collaboration_protocol.md`: this file.

## Workflow

1. Planner writes `taskN-plan.md`.
2. Worker implements the task.
3. Worker writes `taskN-summary.md`.
4. Planner reviews code, docs, and tests.
5. Planner writes `taskN-review.md`.
6. If review says `CHANGES_REQUESTED`, worker updates the implementation and
   appends a revision section to `taskN-summary.md`.
7. If review says `APPROVED`, planner writes the next task plan.

## Summary Requirements

Each worker summary must include:

- Files changed.
- Behavior changed.
- Tests/build commands run.
- Exact pass/fail results.
- Known limitations or follow-up items.
- Any deliberate deviation from the plan, with reason.

## Review Requirements

Each review must include:

- Decision: `APPROVED`, `CHANGES_REQUESTED`, or `BLOCKED`.
- Findings ordered by severity.
- Required fixes, if any.
- Verification performed by reviewer, if any.
- Next recommended task when approved.

## Engineering Rules

- Preserve the microservice direction.
- Do not collapse service boundaries for convenience.
- Use ODB for PostgreSQL persistence work unless explicitly redirected.
- Redis/PostgreSQL development dependencies run through Docker Compose.
- Every new module or behavior needs focused tests.
- Avoid broad refactors unless the task explicitly calls for them.
- Do not revert unrelated user or agent changes.
- Keep docs current when behavior or architecture changes.
