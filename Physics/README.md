# Physics and components

`App` owns one `Physics` service. `Physics::GetInstance()` returns that instance
and throws `std::logic_error` before construction or after destruction. Only one
App may be active in a process. Access and stepping are confined to the main
thread; this is not a process-lifetime lazy singleton.

The service creates a Box3D world from `b3DefaultWorldDef()`, with gravity
`(0, -9.8, 0)` and one worker. Other settings keep their library defaults.
`Step()` always advances `1/60` second with four internal substeps. These are
solver substeps, not four script updates. Physics exposes no native world handle.

App schedules fixed ticks with `FixedStepClock`, independently of rendering.
Each tick runs script Awake/Start, script FixedUpdate, physics, transform readback,
scene Update, then LateUpdate. Physics and scripts run only in unpaused Play mode. Existing
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

The default demonstration scene still starts with an empty world. Add Rigidbody
and Collider through the inspector's Add Component menu or `AddComponent<T>()`.
Reset restores the existing demonstration scene; it does not restore a snapshot
of editor changes.

## Components and ownership

Each GameObject supports one active `Rigidbody` and one active `Collider`.
Rigidbody defaults to Static and owns its private body handle. Switching between
Static and Dynamic destroys and recreates the body at its current world pose,
reattaches its collider, retains tuning, and resets linear/angular motion.
The GameObject renderer/BVH Static flag is independent of the Rigidbody type.

Collider owns its shape and creates an internal static body when used alone.
Adding a Rigidbody replaces that fallback; removing a Rigidbody restores it.
Removing a Collider leaves the Rigidbody intact, with a visible missing-collider
warning. No substitute shape is created. Body user data always points to the
owning GameObject. Native body, shape, and world handles remain internal.

Resources are initialized after component ownership is assigned. Pending
removals are excluded from lookup and duplicate checks and release physics
resources immediately, so scripts may remove/re-add a component in one tick.
Destroying a GameObject releases its entire subtree's native resources immediately
without creating fallback bodies; C++ component destruction remains deferred.
Scene cleanup must precede `Physics::Reset()` and Physics destruction.

## Shape settings

The default collider is a centered unit box. Sphere radius defaults to 0.5;
capsules use local Y, radius 0.5, and total height 2 (including both caps).
Boxes use a copied Box3D hull; spheres and capsules use native primitives.
Box dimensions and center scale per axis. Spheres use the largest world scale
axis for radius. Capsules use the larger X/Z scale for radius and Y for total
height, clamped to at least the diameter. A capsule without a cylindrical segment
uses the equivalent sphere primitive.

Geometry edits rebuild the shape and its mass properties. Density edits update
mass immediately. Body gravity scale, damping, motion locks, density, friction,
and restitution start with Box3D defaults and are editable in the inspector.
Public configuration setters return false and retain their previous settings for
invalid input: dimensions/density must be positive, damping/friction nonnegative,
restitution in [0, 1], and all numeric inputs finite. Gravity scale may be negative.

## Transforms and hierarchy

`GetTransform()` is read-only. Use `SetPosition`, `SetRotation`, `SetScale`, or
`SetTransform`; inspector and gizmo edits use those setters too. They immediately
update the effective native body's world pose, wake dynamic bodies, and rebuild
collider geometry when scale changes. Explicit parent edits propagate to all
descendants. Reparenting retains the existing local transform semantics and
immediately updates affected bodies.

After every physics step, Scene snapshots every active component body's world
position and normalized quaternion, including static and sleeping bodies. It
applies those snapshots parent-first through a private path that preserves local
scale and does not write back into Box3D. Consequently, simulated parents do not
drag independent physics children. Scripts see the new transforms in Update and
LateUpdate of the same tick. Paused physics remains stationary, while explicit
editor changes still take effect immediately.

Physics requires finite transforms, positive object scale, and positive uniform
scale on every ancestor. Unsupported transforms disable the affected body and
show inspector warnings. Correcting the transform rebuilds geometry as needed
and enables the body at the current GameObject pose. Euler/quaternion conversion
uses DirectX's existing rotation convention.

Kinematic bodies, sensors, multiple colliders, collision events/queries, public
force/velocity controls, debug drawing, and interpolation remain deferred.

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
./tests/RunPhysicsTests.ps1 -Configuration Debug -Suite All
./tests/RunPhysicsTests.ps1 -Configuration Release -Suite All
./tests/RunAnimationTests.ps1 -Configuration Debug -Suite All
./tests/RunAnimationTests.ps1 -Configuration Release -Suite All
```

The App smoke suite uses the actual App, its default scene assets, a hidden
window, and D3D11. It exercises Play/Pause/Resume/Stop state changes, Escape
input, rendering while paused, script/physics ordering, and component teardown
before world reset/shutdown. `-Suite Components` runs the headless component
suite, including contacts for all shapes, attachment/removal, geometry/mass,
hierarchy/sleep, invalid-transform recovery, and ImGui text edits plus the gizmo
world-transform entry point. Native handles are exposed only by private friend
test accessors. `-Suite All` runs Core, Components, and App. Results and binaries
are written under `x64/PhysicsTests/<configuration>/`.

References: [Box3D overview](https://box2d.org/documentation3d/index.html),
[simulation guide](https://box2d.org/documentation3d/md_simulation.html), and
[Hello Box3D](https://box2d.org/documentation3d/hello.html).
