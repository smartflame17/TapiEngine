# Skeletal animation pipeline

The engine evaluates bone poses on the CPU and deforms vertices on the GPU.
`Animator` controls time and clip selection, `Model` owns the evaluated render
pose, and `Mesh` uploads the skinning matrices used by its vertex shader.
The same pose is used for lighting, shadows, culling, and picking.

## Structure and ownership

| Type / subsystem | Responsibility | Ownership |
| --- | --- | --- |
| `ModelAsset` (`Graphics/Assets/ModelAsset.h`) | CPU geometry, materials, hierarchy, and skin bindings | Immutable data that can be shared by model instances |
| `Skeleton` / `SkeletonNode` | Complete parent-before-child node hierarchy, bind-local matrices, decomposed bind S/R/T, and bone node indices | Part of the model asset; source hierarchy is also recorded in clips |
| `MeshSkinBinding` | Per-mesh bone-to-node indices, inverse-bind matrices, and influence bounds | Part of each mesh asset |
| `AnimationClip` / `NodeTrack` | Named translation, quaternion rotation, and scale keys, with timestamps in seconds | Immutable and independent of any target model |
| `ClipBinding` | Maps target node indices to clip track indices | Per Animator clip entry; does not alter the shared clip |
| `PlaybackClock` | Playing, Paused, Stopped, and Completed states; time, speed, and loop advancement | Per Animator |
| `Animator` (`Components/Animator.*`) | Clip loading, compatibility checks, script/editor controls, and pose evaluation | One component per GameObject, targeting its single skinned Model |
| `SkeletonPose` | Local and accumulated global node matrices | Animator evaluation scratch data, copied into each Model |
| `Model` / `Mesh` | GPU geometry, pose-dependent palettes, and local bounds | Per rendered instance |
| `SkinningCbuf` | Dirty CPU constants and the dynamic VS constant buffer | Per skinned mesh instance, reused across passes |
| `Renderer` / `ShadowDrawContext` | Render passes and selection of rigid or skinned shadow shaders | Renderer |

Assimp appears only in `Graphics/Assets/AssimpImporter.cpp`. Neither the pose
evaluator nor Animator, Model, or the shaders depend on Assimp scene pointers.
`ImportModel(path)` and `ImportAnimations(path)` copy the data before their local
importers are destroyed. A future custom-format reader can produce these same
engine-owned types. `Model(Graphics&, path)` remains the convenient loading API;
`Model(Graphics&, shared_ptr<const ModelAsset>)` accepts already loaded data.

## One-time loading and binding

1. **Import the model.** `ImportModel` uses triangulation, smooth normals, tangent
   generation, vertex joining, and left-handed conversion. FBX pivot preservation
   is disabled consistently for both model and animation imports. This prevents
   mismatched Assimp helper-node chains in the supplied Mixamo files.

2. **Copy the hierarchy and skin.** All hierarchy nodes are retained, including
   non-bone parents and mesh nodes. Each mesh stores its own inverse binds:
   sharing a bone name does not imply two meshes have the same inverse bind.
   Influences are sorted, reduced to the strongest four, and normalized. Excess
   influences produce a warning. Zero-weight vertices follow the mesh node
   rigidly. More than 128 bones in one mesh is an import error.

3. **Create the GPU model.** `Model::CreateMesh` creates buffers and materials
   from `MeshAsset`. Skinned layouts append `BLENDINDICES` (`uint4`) and
   `BLENDWEIGHT` (`float4`). The vertex shader is selected according to whether
   the mesh is skinned and has UVs. Static meshes keep their original shaders.
   The initial pose is the model's bind pose.

4. **Import clips.** `Animator::LoadAnimations` imports every clip in the selected
   file. Animation-only FBXs may have zero meshes and an Assimp "incomplete"
   flag; that is valid here. Keys and duration are converted from ticks to
   seconds. Missing tick rates use 30 ticks/second with a warning. Unsupported
   curves, morph/mesh animation, invalid timestamps, and malformed values are
   rejected. Cubic curves must be baked before export.

5. **Bind each clip to the target.** `BindClip` matches exact node names, parents,
   and compatible bind transforms, including ancestors of affected nodes.
   Unrelated mesh nodes need not exist in the animation file. Small export
   differences are tolerated, but the engine does not retarget different rigs
   or strip name prefixes. The source path and animation index form the clip ID;
   filename-qualified labels distinguish clips that share a name such as
   `mixamo.com`. Loading is staged so a failed import/binding preserves existing
   clips and playback.

## Per-frame order

1. **Capture elapsed time.** `App::Begin` obtains `frameDelta` from the timer and
   performs the existing fixed-step simulation updates. After those updates it
   calls `Scene::UpdateAnimations(frameDelta, isPlayMode, isPaused)` once, before
   `RenderFrame`. Animator does not advance through fixed-step `OnUpdate`.

2. **Resolve live components.** The scene traverses live GameObjects, including
   offscreen objects. Each Animator locates its current, non-pending skinned
   DrawableComponent. Removed or ambiguous targets disable advancement. A new
   drawable ID or asset causes clips to be rebound. Only immutable asset data
   and a component ID are retained; the target Model pointer is used transiently.

3. **Advance the playback clock.** An Animator in Playing state advances by
   `frameDelta * speed` in both Edit and Play modes, without a preview toggle.
   Global pause blocks advancement only when the editor is in Play mode.
   Animator Pause/Stop still work in either mode. Loops wrap time; non-looping
   clips hold their final pose in Completed state. Pausing preserves time and
   the existing GPU pose. Explicit seeking can update the pose while paused.

4. **Sample local transforms.** `EvaluatePose` locates neighboring keys by time.
   Translation and scale use linear interpolation; rotations use normalized
   shortest-path quaternion slerp. Step keys hold the left value. Missing tracks
   use the target's bind values; single keys are constant; samples beyond a
   track's endpoints hold its nearest key. Unanimated nodes retain their exact
   bind-local matrix.

5. **Accumulate the hierarchy.** CPU matrices use DirectX row-vector convention:

   ```text
   local[node]  = Scale * Rotation * Translation
   global[node] = local[node] * global[parent]
   ```

   A root's global matrix is its local matrix. These globals are in the imported
   model hierarchy's coordinate space; they do not contain the GameObject's
   world transform. Animated bone translation stays in the rig and does not
   move the GameObject or drive physics.

6. **Copy the pose and prepare each mesh.** `Model::SetPose` owns a copy of the
   result and skips geometry updates if its global matrices are unchanged.
   `Model::UpdateGeometry` computes one palette entry for each mesh bone:

   ```text
   skin[b] = inverseBind[b] * global[boneNode[b]] * inverse(global[meshNode])
   ```

   This transforms a bind vertex into the animated mesh's local space. The
   regular mesh draw subsequently applies `global[meshNode]`, followed by the
   Model/GameObject world transform. Do not apply another root inverse or apply
   the mesh node twice. The palette is approximately identity in bind pose
   (subject to source-file precision).

7. **Update animated bounds.** At import time, an AABB is built from the vertices
   influenced by each bone after weight reduction. Each frame, these bounds are
   transformed by `inverseBind * global[bone]` and merged in model space. Rigid
   vertices and unskinned meshes contribute their mesh-node-transformed bounds.
   Positive, normalized weights keep each blended vertex inside the union's
   enclosing AABB. This is conservative and avoids CPU skinning every vertex.

8. **Synchronize spatial data and build the render queue.** `Renderer::Render`
   asks `Scene::Submit` to populate the queue. `BVHManager::Sync` reads the new
   bounds through `DrawableComponent::GetWorldBounds`, which applies the
   object's world transform. The camera frustum query therefore sees the
   animated shape. Picking also synchronizes and queries these bounds. Since
   animation updates happen before culling, an offscreen character can animate
   back into view.

9. **Upload and draw shadows.** The renderer runs its directional, spot, and
   point-light shadow passes. Each draw receives a `ShadowDrawContext` with
   rigid and skinned shaders. Mesh selects and binds the proper shader and
   matching input layout on every draw, including when rigid and skinned meshes
   alternate. A dirty `SkinningCbuf` uploads using `WRITE_DISCARD` on its first
   bind; later shadow faces and lighting passes reuse the buffer. The skinned
   shadow shader uses the same position-deformation helper as the lit shaders.

10. **Draw lighting passes.** The opaque base and additive-light passes bind the
    mesh's material and regular/skinned Phong vertex shader. Skinned positions
    are a weighted sum of the four bone-transformed positions. Normals use
    inverse-transpose bone matrices and the inverse-transpose world matrix;
    tangents use the linear bone/world transforms, then are orthogonalized
    against the normal. The resulting position is transformed to world and
    clip space. Existing pixel shaders perform texturing and lighting.

11. **Draw the editor and clean up.** ImGui uses the same Animator methods that
    scripts call. The inspector runs after scene rendering, so edits become
    visible on the following render. Pending components are then removed.
    On the next animation update, absence of a valid Animator resets the
    remaining Model to bind pose and refreshes its bounds. Drawable removal
    unregisters its BVH entry; neither Animator nor the render pose retains a
    dangling drawable pointer.

## Shader constants

| Stage / slot | Contents |
| --- | --- |
| VS `b0`, lit draw | Transposed model-view-projection, model/world, and normal/world matrices |
| VS `b0`, shadow draw | Transposed model-light-view-projection matrix |
| VS `b1`, skinned draw | 128 position matrices followed by 128 normal matrices (16 KiB) |

`PrepareSkinningConstants` transposes CPU matrices for HLSL's default
column-major constant-buffer packing. CPU hierarchy calculations themselves
are not performed on upload-transposed matrices. Pixel-shader slots are
separate from vertex-shader slots, so VS `b1` does not conflict with PS lighting
constants. `Shaders/Skinning.hlsli` is shared by all three skinned vertex shaders.

## Controls and current limits

New Animators start Stopped at time zero and speed 1. Loading a clip does not
automatically press Play. Play starts/resumes, Restart resets and plays, Pause
holds the pose, Stop restores bind pose, and Seek displays a time in Paused
state. Switching clips resets time while preserving Playing/Paused/Stopped as
appropriate. Removing the active clip stops playback; removing another clip
preserves it. Invalid clips can be removed even after a target rig replacement.
Global scene Stop retains the engine's existing behavior of rebuilding the scene.

The implementation supports one clip at a time, four influences per vertex,
and 128 bones per mesh. Animated/bind rig scales must be positive and nonsingular;
nonuniform positive scale is supported. Static node controls remain available;
manual node-pose controls are disabled for rigged Models. Cross-fades, IK,
retargeting, root-motion extraction, serialization, and disk caches are not
implemented.

## Verification

Build the engine in Debug or Release x64 first, then run from the repository root:

```powershell
./tests/RunAnimationTests.ps1 -Configuration Debug
./tests/RunAnimationTests.ps1 -Configuration Release
```

`-Suite Cpu`, `-Suite Gpu`, and `-Suite Integration` select individual suites.
Integration tests link the engine's objects from the selected configuration,
so rebuild after source changes. Tests cover interpolation/rig validation,
playback controls, component removal/replacement, picking, moving bounds,
offscreen animation entering the frustum, actual vertex-shader readback, and
moving shadow silhouettes in 2D and six cube-map faces. Synthetic fixtures run
without external assets. If the locally ignored Humanoid files exist, tests
also import both clips, render them with the engine, and check the animated
bounds against CPU-skinned vertices. Render captures are written under
`x64/AnimationTests/<configuration>/`.
