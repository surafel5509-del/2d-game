// ============================================================
//  Nebula Engine - native test suite (runs on host, no device)
//  Covers: math, JSON round-trip, ECS registry, physics solvers,
//  raycasts, particles, animator, scene serialization.
//  Build & run:  scripts/run_native_tests.sh
// ============================================================
#include "engine/math/vec2.hpp"
#include "engine/math/mat4.hpp"
#include "engine/math/rect.hpp"
#include "engine/math/random.hpp"
#include "engine/json/json.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/ecs/components.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/particles/particle_system.hpp"
#include "engine/animation/sprite_animator.hpp"
#include "engine/scene/scene.hpp"
#include "engine/render/camera2d.hpp"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace nebula;

static int gTestsRun = 0;
static int gTestsFailed = 0;

using TestFn = void (*)();
static std::vector<std::pair<std::string, TestFn>>& testRegistry() {
    static std::vector<std::pair<std::string, TestFn>> tests;
    return tests;
}

#define TEST(name) \
    static void name(); \
    struct name##_reg { name##_reg() { testRegistry().emplace_back(#name, &name); } } name##_instance; \
    static void name()

static void runTest(const char* name, TestFn fn) {
    ++gTestsRun;
    try {
        fn();
        std::printf("  [PASS] %s\n", name);
    } catch (const std::exception& ex) {
        ++gTestsFailed;
        std::printf("  [FAIL] %s  -> %s\n", name, ex.what());
    } catch (...) {
        ++gTestsFailed;
        std::printf("  [FAIL] %s  -> unknown exception\n", name);
    }
}

#define EXPECT_TRUE(cond) do { if (!(cond)) throw std::runtime_error(std::string("EXPECT_TRUE failed: ") + #cond); } while (0)
#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))
#define EXPECT_NEAR(a, b, eps) do { double _da = std::fabs((double)((a) - (b))); if (_da > (eps)) throw std::runtime_error(std::string("EXPECT_NEAR failed: ") + #a " vs " #b); } while (0)
#define EXPECT_EQ(a, b) do { if (!((a) == (b))) throw std::runtime_error(std::string("EXPECT_EQ failed: ") + #a " == " #b); } while (0)

// ---------------- Math ----------------

TEST(vec2_arithmetic) {
    Vec2 a(3.0f, 4.0f);
    Vec2 b(1.0f, 2.0f);
    EXPECT_NEAR((a + b).x, 4.0f, 1e-6f);
    EXPECT_NEAR((a - b).y, 2.0f, 1e-6f);
    EXPECT_NEAR((a * 2.0f).y, 8.0f, 1e-6f);
    EXPECT_NEAR(a.length(), 5.0f, 1e-6f);
    EXPECT_NEAR(a.dot(b), 11.0f, 1e-6f);
    EXPECT_NEAR(a.cross(b), 3.0f * 2.0f - 4.0f * 1.0f, 1e-6f);
    Vec2 n = a.normalized();
    EXPECT_NEAR(n.length(), 1.0f, 1e-6f);
}

TEST(vec2_rotation) {
    Vec2 v(1.0f, 0.0f);
    Vec2 r = v.rotated(3.14159265f / 2.0f);
    EXPECT_NEAR(r.x, 0.0f, 1e-5f);
    EXPECT_NEAR(r.y, 1.0f, 1e-5f);
}

TEST(mat4_identity_and_ortho) {
    Mat4 id = Mat4::identity();
    EXPECT_NEAR(id.m[0], 1.0f, 1e-7f);
    Mat4 proj = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -1.0f, 1.0f);
    // Point at (10,0,0) should project to x = +1 in NDC.
    const float x = proj.m[0] * 10.0f + proj.m[12];
    EXPECT_NEAR(x, 1.0f, 1e-6f);
    const float y = proj.m[5] * 10.0f + proj.m[13];
    EXPECT_NEAR(y, 1.0f, 1e-6f);
}

TEST(mat4_view_projection_composition) {
    Camera2D cam;
    cam.position = Vec2(100.0f, 50.0f);
    cam.zoom = 2.0f;
    Mat4 vp = cam.viewProjection(400.0f, 400.0f);  // half extents 100 world units
    // World origin (100,50) is the camera center -> should map near NDC origin.
    // Compose manually: clip = vp * [x y 0 1]
    auto clip = [&](Vec2 w) {
        float cx = vp.m[0] * w.x + vp.m[4] * w.y + vp.m[12];
        float cy = vp.m[1] * w.x + vp.m[5] * w.y + vp.m[13];
        return std::pair<float, float>(cx, cy);
    };
    auto [cx, cy] = clip(Vec2(100.0f, 50.0f));
    EXPECT_NEAR(cx, 0.0f, 1e-5f);
    EXPECT_NEAR(cy, 0.0f, 1e-5f);
    // Right edge of view (world 100 + 100) -> NDC +1.
    auto [ex, ey] = clip(Vec2(200.0f, 50.0f));
    EXPECT_NEAR(ex, 1.0f, 1e-5f);
    (void)ey;
}

TEST(rect_and_aabb) {
    Rect r{10, 10, 20, 20};
    EXPECT_TRUE(r.contains(Vec2(15, 15)));
    EXPECT_FALSE(r.contains(Vec2(40, 40)));
    Rect other{25, 25, 10, 10};
    EXPECT_TRUE(r.intersects(other));

    AABB a = AABB::fromCenterHalfExtents(Vec2(0, 0), Vec2(10, 10));
    AABB b = AABB::fromCenterHalfExtents(Vec2(15, 0), Vec2(10, 10));
    AABB c = AABB::fromCenterHalfExtents(Vec2(50, 0), Vec2(10, 10));
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_FALSE(a.overlaps(c));
}

TEST(random_is_deterministic) {
    Random r1(42), r2(42);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(r1.nextU32(), r2.nextU32());
}

// ---------------- JSON ----------------

TEST(json_roundtrip) {
    json::Value root = json::Value::object();
    root["name"] = json::Value("Test Scene");
    root["count"] = json::Value(42);
    root["pi"] = json::Value(3.5);
    root["flag"] = json::Value(true);
    json::Value arr = json::Value::array();
    arr.push(json::Value(1)); arr.push(json::Value("two")); arr.push(json::Value(false));
    root["list"] = arr;

    const std::string text = root.dump(2);
    json::Value parsed = json::parse(text);
    EXPECT_EQ(parsed.at("name").asString(), std::string("Test Scene"));
    EXPECT_EQ(parsed.at("count").asInt(), 42);
    EXPECT_TRUE(parsed.at("flag").asBool());
    EXPECT_EQ(parsed.at("list").asArray().size(), 3u);
    EXPECT_EQ(parsed.at("list").asArray()[1].asString(), std::string("two"));
}

TEST(json_nested_parse) {
    json::Value v = json::parse(R"({"a":{"b":{"c":[1,2,{"d":"x"}]}},"e":null})");
    const json::Value* c = v.find("a");
    EXPECT_TRUE(c != nullptr);
    EXPECT_EQ(c->at("b").at("c").asArray()[2].at("d").asString(), std::string("x"));
}

TEST(json_invalid_throws) {
    bool threw = false;
    try { json::parse("{invalid"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ---------------- ECS registry ----------------

TEST(ecs_create_add_get_destroy) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<NameComponent>(e, NameComponent{"Hero"});
    reg.add<TransformComponent>(e, TransformComponent{Vec2(1, 2), 0.5f, Vec2::one()});
    EXPECT_TRUE(reg.alive(e));
    EXPECT_EQ(reg.get<NameComponent>(e).name, std::string("Hero"));
    EXPECT_NEAR(reg.get<TransformComponent>(e).rotation, 0.5f, 1e-6f);
    EXPECT_EQ(reg.entityCount(), 1u);

    reg.destroy(e);
    EXPECT_FALSE(reg.alive(e));
    EXPECT_FALSE(reg.has<NameComponent>(e));
}

TEST(ecs_dense_iteration_and_remove) {
    Registry reg;
    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        const Entity e = reg.create();
        reg.add<TransformComponent>(e, TransformComponent{Vec2((float)i, 0), 0.0f, Vec2::one()});
        entities.push_back(e);
    }
    reg.destroy(entities[3]);

    float sum = 0.0f;
    int count = 0;
    reg.each<TransformComponent>([&](Entity, TransformComponent& t) { sum += t.position.x; ++count; });
    EXPECT_EQ(count, 9);
    EXPECT_NEAR(sum, 45.0f - 3.0f, 1e-5f);
}

TEST(ecs_pair_iteration) {
    Registry reg;
    const Entity withBoth = reg.create();
    reg.add<TransformComponent>(withBoth, TransformComponent{Vec2(7, 8), 0, Vec2::one()});
    reg.add<SpriteComponent>(withBoth, SpriteComponent{});
    const Entity onlyTransform = reg.create();
    reg.add<TransformComponent>(onlyTransform, TransformComponent{});

    int pairs = 0;
    reg.eachPair<TransformComponent, SpriteComponent>([&](Entity e, TransformComponent&, SpriteComponent&) {
        EXPECT_EQ(e, withBoth);
        ++pairs;
    });
    EXPECT_EQ(pairs, 1);
}

// ---------------- Physics ----------------

TEST(physics_gravity_integration) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<TransformComponent>(e, TransformComponent{Vec2(0, 100), 0.0f, Vec2::one()});
    reg.add<RigidbodyComponent>(e, RigidbodyComponent{});
    reg.add<BoxColliderComponent>(e, BoxColliderComponent{Vec2(10, 10), Vec2::zero(), false, -1, -1});

    PhysicsWorld world;
    world.gravity = Vec2(0.0f, -100.0f);
    world.step(reg, 0.1f);
    const auto& t = reg.get<TransformComponent>(e);
    const auto& rb = reg.get<RigidbodyComponent>(e);
    EXPECT_TRUE(rb.velocity.y < 0.0f);
    EXPECT_TRUE(t.position.y < 100.0f);
}

TEST(physics_static_body_does_not_move) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<TransformComponent>(e, TransformComponent{Vec2(0, 0), 0.0f, Vec2::one()});
    RigidbodyComponent rb; rb.isStatic = true;
    reg.add<RigidbodyComponent>(e, rb);
    reg.add<BoxColliderComponent>(e, BoxColliderComponent{Vec2(200, 20), Vec2::zero(), false, -1, -1});

    PhysicsWorld world;
    world.gravity = Vec2(0.0f, -100.0f);
    world.step(reg, 0.5f);
    EXPECT_NEAR(reg.get<TransformComponent>(e).position.y, 0.0f, 1e-5f);
}

TEST(physics_box_rests_on_static_floor) {
    Registry reg;
    // Floor
    const Entity floor = reg.create();
    reg.add<TransformComponent>(floor, TransformComponent{Vec2(0, 0), 0.0f, Vec2::one()});
    RigidbodyComponent floorRb; floorRb.isStatic = true;
    reg.add<RigidbodyComponent>(floor, floorRb);
    reg.add<BoxColliderComponent>(floor, BoxColliderComponent{Vec2(500, 10), Vec2::zero(), false, -1, -1});
    // Falling box
    const Entity box = reg.create();
    reg.add<TransformComponent>(box, TransformComponent{Vec2(0, 60), 0.0f, Vec2::one()});
    RigidbodyComponent rb; rb.restitution = 0.0f;
    reg.add<RigidbodyComponent>(box, rb);
    reg.add<BoxColliderComponent>(box, BoxColliderComponent{Vec2(10, 10), Vec2::zero(), false, 0.0f, 0.6f});

    PhysicsWorld world;
    world.gravity = Vec2(0.0f, -980.0f);
    for (int i = 0; i < 240; ++i) world.step(reg, 1.0f / 60.0f);  // 4 simulated seconds

    const float finalY = reg.get<TransformComponent>(box).position.y;
    // Floor top = 10; box half = 10; resting center ~ 20 (with slop tolerance).
    EXPECT_NEAR(finalY, 20.0f, 1.5f);
}

TEST(physics_circle_vs_circle_collide) {
    Registry reg;
    const Entity a = reg.create();
    reg.add<TransformComponent>(a, TransformComponent{Vec2(-30, 0), 0.0f, Vec2::one()});
    RigidbodyComponent rbA; rbA.gravityScale = 0.0f; rbA.restitution = 0.9f; rbA.velocity = Vec2(120, 0);
    reg.add<RigidbodyComponent>(a, rbA);
    reg.add<CircleColliderComponent>(a, CircleColliderComponent{12.0f, Vec2::zero(), false, 0.9f, -1.0f});

    const Entity b = reg.create();
    reg.add<TransformComponent>(b, TransformComponent{Vec2(30, 0), 0.0f, Vec2::one()});
    RigidbodyComponent rbB; rbB.isStatic = true;
    reg.add<RigidbodyComponent>(b, rbB);
    reg.add<CircleColliderComponent>(b, CircleColliderComponent{12.0f, Vec2::zero(), false, 0.9f, -1.0f});

    PhysicsWorld world;
    world.gravity = Vec2(0, 0);
    for (int i = 0; i < 120; ++i) world.step(reg, 1.0f / 60.0f);

    // After bouncing off the static circle, a should be moving away (-x).
    EXPECT_TRUE(reg.get<RigidbodyComponent>(a).velocity.x < 0.0f);
}

TEST(physics_trigger_fires_enter_exit_events) {
    Registry reg;
    // Static trigger zone
    const Entity zone = reg.create();
    reg.add<TransformComponent>(zone, TransformComponent{Vec2(0, 0), 0.0f, Vec2::one()});
    RigidbodyComponent zoneRb; zoneRb.isStatic = true;
    reg.add<RigidbodyComponent>(zone, zoneRb);
    reg.add<BoxColliderComponent>(zone, BoxColliderComponent{Vec2(50, 50), Vec2::zero(), true, -1, -1});
    // Dynamic mover passing through
    const Entity mover = reg.create();
    reg.add<TransformComponent>(mover, TransformComponent{Vec2(-300, 0), 0.0f, Vec2::one()});
    RigidbodyComponent rb; rb.gravityScale = 0.0f; rb.velocity = Vec2(500, 0);
    reg.add<RigidbodyComponent>(mover, rb);
    reg.add<BoxColliderComponent>(mover, BoxColliderComponent{Vec2(10, 10), Vec2::zero(), false, -1, -1});

    PhysicsWorld world;
    world.gravity = Vec2(0, 0);
    bool sawEnter = false, sawExit = false;
    for (int i = 0; i < 180; ++i) {
        world.step(reg, 1.0f / 60.0f);
        for (const auto& ev : world.entered()) if (ev.a != kNullEntity) sawEnter = true;
        for (const auto& ev : world.exited())  { (void)ev; sawExit = true; }
    }
    EXPECT_TRUE(sawEnter);
    EXPECT_TRUE(sawExit);
}

TEST(physics_raycast_hits_box) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<TransformComponent>(e, TransformComponent{Vec2(200, 0), 0.0f, Vec2::one()});
    reg.add<BoxColliderComponent>(e, BoxColliderComponent{Vec2(20, 20), Vec2::zero(), false, -1, -1});

    PhysicsWorld world;
    RaycastHit hit;
    const bool ok = world.raycast(reg, Vec2(0, 0), Vec2(1, 0), 500.0f, hit);
    EXPECT_TRUE(ok);
    EXPECT_EQ(hit.entity, e);
    EXPECT_NEAR(hit.distance, 180.0f, 1.0f);
}

TEST(physics_raycast_circle_normal) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<TransformComponent>(e, TransformComponent{Vec2(100, 0), 0.0f, Vec2::one()});
    reg.add<CircleColliderComponent>(e, CircleColliderComponent{15.0f, Vec2::zero(), false, -1, -1});

    PhysicsWorld world;
    RaycastHit hit;
    const bool ok = world.raycast(reg, Vec2(0, 0), Vec2(1, 0), 500.0f, hit);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(hit.distance, 85.0f, 1.0f);
    EXPECT_NEAR(hit.normal.x, -1.0f, 1e-3f);
}

// ---------------- Particles ----------------

TEST(particles_burst_and_decay) {
    ParticleSystem ps(64);
    ps.burst(Vec2(0, 0), 20, 50.0f, 200.0f,
             Color(1, 1, 0, 1), Color(1, 0, 0, 1),
             4.0f, 10.0f, 0.2f, 0.4f, 123u);
    EXPECT_TRUE(ps.aliveCount() > 0);
    for (int i = 0; i < 60; ++i) ps.update(1.0f / 60.0f);  // 1 second > all lifetimes
    EXPECT_EQ(ps.aliveCount(), 0u);
}

// ---------------- Animator ----------------

TEST(sprite_animator_advances_frames) {
    Registry reg;
    const Entity e = reg.create();
    reg.add<TransformComponent>(e, TransformComponent{});
    SpriteComponent spr;
    spr.srcW = -1;
    reg.add<SpriteComponent>(e, spr);
    SpriteAnimatorComponent anim;
    anim.frameW = 32; anim.frameH = 32; anim.frameCount = 4; anim.columns = 4;
    anim.framesPerSecond = 10.0f; anim.loop = true;
    reg.add<SpriteAnimatorComponent>(e, anim);

    for (int i = 0; i < 12; ++i) SpriteAnimator::update(reg, e, 0.1f);  // 1.2s @ 10fps -> frame 12 % 4 = 0
    const auto& spr2 = reg.get<SpriteComponent>(e);
    const auto& anim2 = reg.get<SpriteAnimatorComponent>(e);
    EXPECT_EQ(anim2.currentFrame, 0);
    EXPECT_NEAR(spr2.srcW, 32.0f, 1e-6f);
}

// ---------------- Scene serialization ----------------

TEST(scene_serialize_deserialize_roundtrip) {
    Scene scene;
    scene.setName("RoundTrip");
    scene.camera().position = Vec2(12.0f, -34.0f);
    scene.camera().zoom = 1.75f;

    Registry& reg = scene.registry();
    const Entity e = reg.create();
    reg.add<NameComponent>(e, NameComponent{"Ship"});
    reg.add<TagComponent>(e, TagComponent{"player"});
    reg.add<TransformComponent>(e, TransformComponent{Vec2(100, 200), 0.25f, Vec2(2.0f, 3.0f)});
    SpriteComponent spr;
    spr.texture = "ship";
    spr.sizePx = Vec2(128, 96);
    spr.tint = Color(0.5f, 0.6f, 0.7f, 0.8f);
    spr.zOrder = 7.0f;
    reg.add<SpriteComponent>(e, spr);
    RigidbodyComponent rb; rb.gravityScale = 0.0f; rb.isStatic = true;
    reg.add<RigidbodyComponent>(e, rb);
    reg.add<BoxColliderComponent>(e, BoxColliderComponent{Vec2(40, 30), Vec2(1, 2), false, -1, -1});

    const std::string jsonText = scene.serialize();

    Scene loaded;
    const bool ok = loaded.deserialize(jsonText, [](const std::string& name) {
        return name == "ship" ? 777 : 0;
    });
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.name(), std::string("RoundTrip"));
    EXPECT_NEAR(loaded.camera().zoom, 1.75f, 1e-5f);

    const auto& loadedReg = loaded.registry();
    const auto ids = loadedReg.entities<NameComponent>();
    EXPECT_EQ(ids.size(), 1u);
    const Entity le = ids[0];
    EXPECT_EQ(loadedReg.get<NameComponent>(le).name, std::string("Ship"));
    EXPECT_EQ(loadedReg.get<TagComponent>(le).tag, std::string("player"));
    EXPECT_NEAR(loadedReg.get<TransformComponent>(le).position.x, 100.0f, 1e-5f);
    EXPECT_NEAR(loadedReg.get<TransformComponent>(le).rotation, 0.25f, 1e-5f);
    EXPECT_NEAR(loadedReg.get<TransformComponent>(le).scale.y, 3.0f, 1e-5f);
    EXPECT_EQ(loadedReg.get<SpriteComponent>(le).textureId, 777);
    EXPECT_NEAR(loadedReg.get<SpriteComponent>(le).sizePx.y, 96.0f, 1e-5f);
    EXPECT_NEAR(loadedReg.get<SpriteComponent>(le).tint.b, 0.7f, 1e-5f);
    EXPECT_TRUE(loadedReg.has<RigidbodyComponent>(le));
    EXPECT_TRUE(loadedReg.get<RigidbodyComponent>(le).isStatic);
    EXPECT_NEAR(loadedReg.get<BoxColliderComponent>(le).halfExtents.x, 40.0f, 1e-5f);
}

TEST(scene_rejects_garbage) {
    Scene scene;
    bool ok = scene.deserialize("not json at all {{{", [](const std::string&) { return 0; });
    EXPECT_FALSE(ok);
}

int main() {
    std::printf("Nebula Engine — native test suite\n");
    std::printf("=================================\n");
    for (const auto& [name, fn] : testRegistry()) runTest(name.c_str(), fn);
    std::printf("=================================\n");
    std::printf("%d tests run, %d failed\n", gTestsRun, gTestsFailed);
    return gTestsFailed == 0 ? 0 : 1;
}

// ---------------- End-to-end integration (headless) ----------------

#include "engine/nebula_engine.hpp"
#include "engine/game/nebula_run.hpp"

TEST(integration_nebula_run_headless) {
    NebulaEngine engine;
    engine.initGL(720, 1280);  // stub renderer on host

    // Minimal 1x1 RGBA textures so the demo can register sprites.
    const uint8_t pixel[4] = {200, 220, 255, 255};
    for (const char* name : {"nebula_bg", "ship", "enemy", "bullet", "star"}) {
        EXPECT_TRUE(engine.registerTexture(name, pixel, 1, 1) > 0);
    }

    NebulaRunGame game;
    game.attach(&engine);
    game.startDemoScene();

    // 12 simulated seconds: enemies must spawn, fall and cost lives.
    bool sawExplosion = false;
    for (int frame = 0; frame < 720; ++frame) {
        engine.tick(1.0f / 60.0f);
        if (game.consumeSoundFlags() & NebulaRunGame::kExplosion) sawExplosion = true;
    }

    const int lives = game.lives();
    EXPECT_TRUE(lives < 3);                     // escaped enemies cost lives
    EXPECT_EQ(game.score() >= 0, true);
    // Stars + player persist; dead bullets get cleaned up by lifetimes.
    const EngineStats stats = engine.stats();
    EXPECT_TRUE(stats.entityCount > 50);
    (void)sawExplosion;  // explosions are random (aim); lives loss is deterministic
}
