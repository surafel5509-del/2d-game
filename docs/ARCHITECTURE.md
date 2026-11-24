# Nebula Engine — Architecture Deep Dive

## Design goals

1. **Own every layer.** No third-party C++ libraries. If it's in the hot path, it's in this repo.
2. **Testable on the host.** Everything except the GL backend compiles and runs on Linux/OS X — that's why the JNI layer is thin and the facade is GL-agnostic (a no-op `GlRenderer` stub backs host tests).
3. **Mobile-first.** One draw call per texture batch, zero-allocation frame loops (scratch buffers reused), cache-friendly ECS, no locks on the render hot path (GL thread owns GPU state; a single mutex guards editor edits).
4. **The editor is a client of the engine API** — exactly like the game is. No special back doors.

## Frame pipeline

```
EngineView.onDrawFrame (GL thread)
  └─ gMutex
      ├─ NebulaEngine::tick(dt)
      │    ├─ updateHook(dt)            // game logic (Nebula Run / sandbox: none)
      │    ├─ PhysicsWorld::step ×2     // substeps for stability
      │    ├─ SpriteAnimator::updateAll
      │    ├─ LifetimeComponent expiry  // bullets / timed FX despawn
      │    └─ ParticleSystem::update
      └─ NebulaEngine::drawFrame()
           ├─ camera.viewProjection(viewport) → uVP
           ├─ background quad (optional, game)
           ├─ editor grid + axes (editor mode only)
           ├─ drawSprites()             // z-sorted, camera-culled, batched
           ├─ particles (soft dots)     // same batch path
           ├─ selection gizmo (editor)  // lines + handle quads
           ├─ collider debug (toggle)   // line pass
           └─ GlRenderer::endFrame()    // final flush
```

## ECS (`ecs/registry.hpp`)

* `Entity` is a `uint32_t`, monotonically increasing — stale ids can never alias live entities.
* Each component type has a `DataPool<T>`: dense `std::vector<T>` + `std::vector<Entity>` + sparse `unordered_map<Entity,size_t>`. Iteration touches dense arrays only.
* Removal is swap-and-pop; `Registry::destroy` walks all pools via type-erased `IDataPool` pointers.
* Pair iteration (`eachPair<A,B>`) always iterates the smaller pool and sparse-looks-up the rest.
* Built-in components: `Transform`, `Sprite` (with source rect & flip), `Name`, `Tag`, `Rigidbody` (gravity scale, restitution, friction, density→mass), `BoxCollider`, `CircleCollider`, `Lifetime`, `SpriteAnimator`.

## Physics (`physics/physics_world.cpp`)

* **Broadphase:** uniform spatial hash (128 px cells) over conservative world AABBs; candidate pairs deduped with a `seen` set.
* **Narrowphase:** AABB×AABB (minimum-translation axis), circle×circle, circle×AABB. Rotated boxes use rotated-extent AABBs — stable and simple on mobile.
* **Solver:** sequential impulses — normal impulse with combined restitution (max), positional correction (80%, 0.5 px slop), tangent friction clamped by the normal impulse cone.
* **Triggers:** overlap without impulse; enter/exit events diffed per step from pair keys (`min<<32|max`).
* **Raycast:** slab method for boxes, quadratic for circles; returns closest hit + face normal.
* **Known limit:** boxes are solved as AABBs (no full SAT/OBB). Documented; sufficient for the editor and arcade games.

## Rendering (`render/`)

* `IRenderer` is the backend seam; `GlRenderer` is the GLES 3.0 implementation.
* One dynamic `GL_ARRAY_BUFFER`, `GL_STREAM_DRAW` per frame, up to 16K vertices queued; flush on texture switch or capacity.
* Vertices are `x,y,z,u,v,rgba` — positions are world-space transformed on the CPU (2D workloads are vertex-cheap), the shader applies only `uVP`.
* Editor grid/axes/gizmos/collider wireframes use a separate line shader with vertex-color lines.
* Textures upload with CPU row-flip so `v=0` is the image bottom, matching y-up world space.

## Scene format & portability

`Scene::serialize()` writes stable-key-order JSON. Sprites store **logical texture names**;
`deserialize()` re-resolves them via a resolver callback the engine provides, so a scene
authored on one device mounts on another unchanged. Selection state, GL ids and particles
are intentionally not serialized.

## JNI bridge (`jni/nebula_jni.cpp`)

* One process-wide `gMutex` serializes engine access; the GL thread renders while the UI thread edits — frames are short, contention is negligible.
* All exported symbols are `Java_com_nebula_engine_NebulaNative_*` 1:1 with `NebulaNative.kt`; ProGuard config keeps them.
* `nativeDrawFrame` measures real vsync `dt` via `steady_clock`, clamped to 100 ms to survive pauses.

## Failure modes & how the engine handles them

* Malformed scene JSON → `deserialize` returns `false`, scene untouched, error logged.
* Texture name miss → sprite is skipped at draw time, never crashes; id re-resolves when the texture appears.
* Missing sound asset → `SoundManager` skips silently.
* `pick()` on empty viewport → returns `0` (`kNullEntity`).
* Oversized `dt` after app switch → clamped, simulation stays stable.
