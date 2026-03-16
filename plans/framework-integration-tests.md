# Framework Integration Tests Plan

## Objective

Create a repo workflow for the JS framework examples in `wasmer-examples/` that:

1. prepares each example to run through a selected runtime binary instead of the host `node`; and
2. later validates that each framework can actually boot a local HTTP server under that selected runtime.

The user-facing entrypoint should be the `Makefile`, not an ad hoc shell command.

This plan is split into two phases:

- Phase 1 is the setup phase.
- Phase 2 is the runtime validation phase.

The immediate focus is Phase 1, but the plan below also lays out how Phase 2 should work so the setup work does not paint us into a corner.

## Repo Facts Confirmed

- EdgeJS is built via the repo `Makefile`.
- The current EdgeJS binary path is `build-edge/edge`.
- The examples live in the `wasmer-examples` git submodule.
- The current `js-*` examples are:
  - `wasmer-examples/js-astro-staticsite`
  - `wasmer-examples/js-docusaurus-staticsite`
  - `wasmer-examples/js-docusaurus2-staticsite`
  - `wasmer-examples/js-docusaurusold-staticsite`
  - `wasmer-examples/js-gatsby-staticsite`
  - `wasmer-examples/js-gatsby-staticsite2`
  - `wasmer-examples/js-next-staticsite`
  - `wasmer-examples/js-remix-staticsite`
  - `wasmer-examples/js-svelte`

## Key Mechanism We Are Exploiting

`pnpm install` creates shell launchers in `node_modules/.bin/`. A launcher like:

`node_modules/.bin/astro`

contains logic equivalent to:

```sh
basedir=$(dirname "$0")

if [ -x "$basedir/node" ]; then
  exec "$basedir/node" ...
else
  exec node ...
fi
```

That means the leverage point is not the framework binary itself. The leverage point is:

`node_modules/.bin/node`

If we create a symlink there that points to the selected runner binary, then the existing `pnpm`-generated framework launchers will execute through that runtime without rewriting each framework command.

## Operator Interface

The intended operator interface is:

- `make framework-test`
- `make framework-test <framework>`
- `make framework-test-reset`

The runner binary should also be configurable via:

- `SYMLINK_TARGET=<path>`

Where:

- `make framework-test` runs the full prepare-and-test workflow for all supported `js-*` examples.
- `make framework-test <framework>` limits the run to one selected framework example.
- `make framework-test-reset` removes generated state so the framework workflow can be re-run from a clean slate.
- `SYMLINK_TARGET` selects which executable `node_modules/.bin/node` should point to during setup and test.

### SYMLINK_TARGET Semantics

- `SYMLINK_TARGET` should default to the repo-local EdgeJS binary at `build-edge/edge`.
- If `SYMLINK_TARGET` is unset, the workflow should behave as an EdgeJS compatibility run.
- If `SYMLINK_TARGET` is set, the workflow should use that executable instead of assuming EdgeJS.
- This is intended to support repeated runs against different runtimes so results can be cross-compared.
- The target should be validated as an existing executable before any symlink injection happens.
- Logs and status output should print the effective symlink target clearly at the start of the run.

### Notes About The Single-Framework Selector

The requirement is for an optional extra make argument that selects one framework.

Examples of the intended UX:

- `make framework-test js-next-staticsite`
- `make framework-test js-svelte`

This is a spec requirement for the command shape. The eventual implementation can satisfy it either by:

- using `MAKECMDGOALS` to treat the extra goal as a framework selector; or
- using a helper target pattern that preserves this CLI shape.

The important point is the user-facing command form, not the internal Make implementation detail.

## Phase 1: Setup

### Goal

Make every `wasmer-examples/js-*` project locally runnable through the selected runner, with EdgeJS as the default target, using one repo-level setup flow.

### Phase 1 Requirements

#### 1. Fast prerequisite checks

The setup flow should begin with cheap checks that fail fast before any expensive work:

- Verify `pnpm` is installed and reachable on `PATH`.
- If `pnpm` is missing, fail with a short actionable message.
- Verify `wasmer-examples/` exists and is populated enough to use.
- Treat an empty, missing, or uninitialized submodule as an error.
- The recovery instruction should be a direct git command, for example:
  `git submodule update --init --recursive wasmer-examples`

These checks are intentionally first because they are fast and remove avoidable failure cases before build or install work starts.

#### 2. Ensure EdgeJS exists

The setup flow should ensure the effective runner target exists and is executable.

Expected behavior:

- Resolve the effective runner path from `SYMLINK_TARGET`, defaulting to `build-edge/edge`.
- If the effective runner is the default `build-edge/edge` path and it already exists and is executable, reuse it.
- If the effective runner is the default `build-edge/edge` path and it does not exist, invoke the repo build through the `Makefile`.
- The default expectation is `make build` from the repo root.
- If the effective runner comes from an explicit `SYMLINK_TARGET`, do not build anything automatically; just verify that the path exists and is executable.
- If build or validation fails, stop immediately.

This keeps the default experience aligned with EdgeJS development while allowing the same harness to target alternate runtimes.

#### 3. Discover target examples

Target discovery should be automatic:

- Search under `wasmer-examples/` for top-level directories matching `js-*`.
- Only include directories that also contain a `package.json`.
- Sort the list deterministically so output is stable between runs.
- Treat zero matches as an error.

This keeps the workflow generic so newly added JS examples are picked up automatically.

#### 4. Install dependencies in parallel

For each discovered example:

- run `pnpm install` from that example directory;
- run these installs in parallel;
- capture per-project logs;
- fail the overall setup if any project install fails.

The setup flow should not move on to launcher injection for a project whose install did not complete successfully.

#### 5. Inject EdgeJS at the shim layer

After install completes for a project:

- inspect `node_modules/.bin/`;
- verify that at least one generated launcher contains the `"$basedir/node"` pattern or equivalent relative-node launcher behavior;
- create or replace `node_modules/.bin/node` as a symlink to the effective `SYMLINK_TARGET`.

The intent is to use the existing `pnpm` launcher behavior rather than mutate `package.json` scripts.

Implementation assumptions for Phase 1:

- the symlink target should come from `SYMLINK_TARGET`, defaulting to `build-edge/edge`;
- re-running setup should refresh the symlink cleanly;
- a project with no compatible launcher pattern should be surfaced explicitly rather than silently marked as prepared.

#### 6. Setup validation

Before Phase 1 reports success, it should verify:

- `node_modules/.bin/` exists for every prepared project;
- `node_modules/.bin/node` exists and resolves to the effective `SYMLINK_TARGET`;
- at least one launcher in that `.bin/` directory appears capable of using the `basedir/node` shortcut.

This is still setup-only validation. It does not yet mean the framework server boots successfully.

### Phase 1 Output

At the end of setup, each JS example should be in this state:

- dependencies installed;
- local launcher shims present;
- `node_modules/.bin/node` pointing at the effective runner target;
- ready for a later runtime test via `pnpm run ...`.

### Phase 1 Success Criteria

Phase 1 is complete when:

- all JS examples were discovered correctly;
- all `pnpm install` runs completed successfully;
- all eligible examples have the runner symlink in the expected shim location;
- the setup flow can be re-run without manual cleanup.

## Reset Behavior

### Goal

Provide a clean-slate recovery path when framework setup or runtime testing leaves behind broken or stale generated state.

### Callsign

The reset entrypoint should be:

- `make framework-test-reset`

### Reset Scope

The reset target should remove any generated state that affects framework-test reproducibility, including:

- the EdgeJS build output used by the default framework workflow, at minimum `build-edge/edge`, and likely the full `build-edge/` directory;
- installed JS dependencies under the targeted example directories, especially `node_modules/`;
- the injected `node_modules/.bin/node` symlinks created for runner interception;
- any framework-test log directories, temp files, pid files, or cached probe artifacts created by the test harness.

### Reset Semantics

- The reset target should leave source-controlled files untouched.
- The reset target should be safe to run even if some generated paths do not exist.
- After reset, `make framework-test` should rebuild and reinstall everything it needs without any manual repair steps.
- If the workflow later supports single-framework selection for reset, that is optional. The baseline requirement is a full reset target.

## Phase 2: Runtime Validation

### Goal

Prove that each prepared example can start a dev or serve-style HTTP process through EdgeJS and respond to at least one local HTTP request.

### Main Problem To Solve

The runtime test phase is harder because two things are not uniform:

- the correct script name to start the framework differs by project;
- the port is not consistently declared up front.

So Phase 2 needs a discovery and launch strategy instead of a single hardcoded command.

### Current Script Inventory

From the current `package.json` files, the likely runtime entrypoints are:

- `js-astro-staticsite`: `dev`, `start`, `preview`
- `js-docusaurus-staticsite`: `start`, `serve`
- `js-docusaurus2-staticsite`: `start`, `serve`
- `js-docusaurusold-staticsite`: `dev`, `start`
- `js-gatsby-staticsite`: `develop`, `start`, `serve`
- `js-gatsby-staticsite2`: `develop`, `start`, `serve`
- `js-next-staticsite`: `dev`, `start`
- `js-remix-staticsite`: `dev`, `start`
- `js-svelte`: `dev`, `preview`

This is the starting inventory for runtime-command selection. It is not yet a validated command matrix.

### Proposed Phase 2 Strategy

#### 1. Prefer explicit port assignment over port discovery

Instead of starting a framework on an unknown port and then trying to discover it, Phase 2 should try to force the port up front.

Benefits:

- simpler readiness checks;
- simpler HTTP probing;
- simpler cleanup;
- less log scraping.

The basic shape should be:

- assign a deterministic free port per project from a reserved local range;
- launch the framework with that chosen port;
- bind to `127.0.0.1` where the framework supports it;
- poll the chosen URL until it responds or times out.

#### 2. Use a script-selection heuristic

The first pass for choosing which script to run should be:

1. prefer a development server script;
2. fall back to a serve script;
3. only use preview where that is the framework's local server mode.

A reasonable initial preference order is:

`dev` -> `develop` -> `start` -> `serve` -> `preview`

This may still need framework-specific overrides.

#### 3. Use framework-specific CLI arg injection where needed

The runtime harness will likely need a small command matrix rather than pretending every framework accepts the same flags.

Current working hypothesis:

- Astro: `pnpm run dev -- --host 127.0.0.1 --port <port>`
- Svelte/Vite: `pnpm run dev -- --host 127.0.0.1 --port <port>`
- Next: `pnpm run dev -- --hostname 127.0.0.1 --port <port>`
- Gatsby: `pnpm run develop -- --host 127.0.0.1 -p <port>`
- Docusaurus 2/3: `pnpm run start -- --host 127.0.0.1 --port <port>`
- Docusaurus 1: `PORT=<port> pnpm run dev`
- Remix: `pnpm run dev -- --port <port>`

These are planning assumptions only. They still need validation when Phase 2 work starts.

#### 4. Detect readiness through HTTP, not just process liveness

A process staying alive is not enough. Runtime success should mean:

- the process started;
- it accepted a TCP listener on the chosen port;
- an HTTP request to the local URL returned a meaningful response.

Likely probe targets:

- `GET /`
- optionally one static asset or framework-specific route if `/` is insufficient

#### 5. Always clean up background processes

Each runtime test should:

- launch the framework in the background;
- capture logs to a per-project file;
- enforce a startup timeout;
- terminate the process on success, failure, or timeout.

### Phase 2 Risks

- Some frameworks may require a build before a serve-style command works.
- Some scripts named `start` are aliases for dev mode, while others assume a prior build.
- CLI flag forwarding through `pnpm run <script> -- ...` may not be consistent across all frameworks.
- Alternate `SYMLINK_TARGET` runtimes may differ in CLI, unsupported builtins, or process behavior, which can change failure modes even when setup succeeds.
- Some frameworks may boot successfully but fail on first HTTP request due to missing browser-only features, unsupported filesystem assumptions, or runtime incompatibilities in EdgeJS.

### Phase 2 Success Criteria

Phase 2 is complete when, for each prepared example:

- the runtime harness can choose a start command;
- the harness can force or reliably know the port;
- the app can be queried over HTTP on localhost;
- logs make failures diagnosable when startup or serving breaks.

## Suggested Final Shape

The eventual workflow should be exposed through `Makefile` targets:

- `framework-test`
- `framework-test-reset`

Internally, those targets may delegate to one helper script with at least two logical stages:

- `setup`
- `test`

The immediate task is still only Phase 1 setup design, but the implementation should be structured so `framework-test` can own the full flow and a later `test` stage can be attached cleanly.

## Non-Goals For The First Implementation

- Do not rewrite framework package scripts.
- Do not hardcode a single framework binary path.
- Do not assume every project uses the same start script.
- Do not claim runtime success at the end of Phase 1.

## Immediate Next Step

Design the first implementation around these public targets:

- `make framework-test`
- `make framework-test <framework>`
- `make framework-test-reset`

Within that shape, Phase 1 should still do the same core work:

- fast checks;
- resolve `SYMLINK_TARGET` and build EdgeJS only when using the default target and it is missing;
- discover target `js-*` examples;
- optionally narrow to one framework when requested;
- run `pnpm install` in parallel for the selected set;
- attach `node_modules/.bin/node -> $SYMLINK_TARGET`;
- verify the shim-based setup succeeded.

Phase 2 should begin only after that setup path is working cleanly.
