# Physics foundation

`App` owns one `Physics` service. `Physics::GetInstance()` returns that instance
and throws `std::logic_error` before construction or after destruction. Only one
App may be active in a process. Access and stepping are confined to the main
thread; this is not a process-lifetime lazy singleton.

The service creates a Box3D world from `b3DefaultWorldDef()`, with gravity
`(0, -9.8, 0)` and one worker. Other settings keep their library defaults.
`Step()` always advances `1/60` second with four internal substeps. These are
solver substeps, not four script updates. Physics exposes no native world handle.

App schedules fixed ticks with `FixedStepClock`, independently of rendering.
Each tick runs script Awake/Start, script FixedUpdate, physics, scene Update,
then LateUpdate. Physics and scripts run only in unpaused Play mode. Existing
editor scene updates and per-frame animation retain their own behavior.
Rendering runs once per outer loop and receives the remaining fractional tick
as alpha; transform interpolation is deferred.

The clock stores time as a double and caps its total accumulated time at 0.25
seconds (15 ticks). Excess time after a stall is discarded. Play, Pause, Resume,
and reset discard time crossing the transition, including partial ticks.
Reset restarts the timer after scene loading, so loading time is not simulated.

Physics is declared before the UI, window, and Scene in App and is therefore
destroyed after them. A second App is rejected before touching shared UI state.
On Stop or Escape, App clears scene components while their world is still valid,
resets the world, then rebuilds the scene. `Physics::Reset()` preserves the old
world if replacement creation fails. World creation failure throws
`std::runtime_error`; no singleton is registered if initial creation fails.

The production world is intentionally empty in this phase. Rigid bodies,
colliders, event processing, GameObject transform synchronization, editor
controls, and interpolation will be integrated separately. Future consumers of
Box3D movement/contact events must process their transient data after each
step, before stepping again.

The x64 project links and copies `box3dd.lib/.dll` in Debug and `box3d.lib/.dll`
in Release. Use the existing matching headers and binaries under `box3d`.

## Validation

Run the headless world-lifetime, falling-body, and fixed-clock tests without
building the graphical engine:

```powershell
./tests/RunPhysicsTests.ps1 -Configuration Debug
./tests/RunPhysicsTests.ps1 -Configuration Release
```

After building the engine in the corresponding x64 configuration, run:

```powershell
./tests/RunPhysicsTests.ps1 -Configuration Debug -Suite App
./tests/RunPhysicsTests.ps1 -Configuration Release -Suite App
```

The App smoke suite uses the actual App, its default scene assets, a hidden
window, and D3D11. It exercises Play/Pause/Resume/Stop state changes, Escape
input, rendering while paused, script/physics ordering, and component teardown
before world reset/shutdown. `-Suite All` runs both suites. Results and binaries
are written under `x64/PhysicsTests/<configuration>/`.

References: [Box3D overview](https://box2d.org/documentation3d/index.html),
[simulation guide](https://box2d.org/documentation3d/md_simulation.html), and
[Hello Box3D](https://box2d.org/documentation3d/hello.html).
