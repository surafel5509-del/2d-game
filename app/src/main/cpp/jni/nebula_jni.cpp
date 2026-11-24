// ============================================================
//  Nebula Engine - JNI bridge (Kotlin <-> C++)
//  All engine access is serialized through one mutex; GL drawing
//  happens on the GLSurfaceView renderer thread, editor edits
//  arrive from the UI thread.
// ============================================================
#include <jni.h>
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>

#include "engine/nebula_engine.hpp"
#include "engine/game/nebula_run.hpp"
#include "engine/core/log.hpp"

using namespace nebula;

namespace {

std::mutex gMutex;
std::unique_ptr<NebulaEngine> gEngine;
std::unique_ptr<NebulaRunGame> gGame;
std::chrono::steady_clock::time_point gLastFrame = std::chrono::steady_clock::now();

NebulaEngine* engine() { return gEngine.get(); }

std::string jstringToStd(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(value, chars);
    return out;
}

jstring stdToJstring(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

Entity checkedEntity(jint id) { return static_cast<Entity>(static_cast<uint32_t>(id)); }

} // namespace

extern "C" {

// ---------------- Lifecycle ----------------

JNIEXPORT jboolean JNICALL Java_com_nebula_engine_NebulaNative_nativeInit(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gEngine) {
        gEngine = std::make_unique<NebulaEngine>();
        gGame = std::make_unique<NebulaRunGame>();
        gGame->attach(gEngine.get());
        NEBULA_LOG_I("Nebula Engine native layer initialized");
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeShutdown(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    gGame.reset();
    if (gEngine) {
        gEngine->shutdownGL();
        gEngine.reset();
    }
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSurfaceCreated(JNIEnv*, jobject, jint width, jint height) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gEngine) gEngine = std::make_unique<NebulaEngine>();
    if (!gGame) { gGame = std::make_unique<NebulaRunGame>(); gGame->attach(gEngine.get()); }
    gEngine->initGL(width, height);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->resizeGL(width, height);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeDrawFrame(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return;
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - gLastFrame).count();
    gLastFrame = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;  // clamp hitches
    engine()->tick(dt);
    engine()->drawFrame();
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeTouchEvent(JNIEnv*, jobject,
                                                                             jint pointerIndex,
                                                                             jint action,
                                                                             jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return;
    engine()->input().pointerEvent(pointerIndex, action, Vec2(x, y), 0);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeKeyEvent(JNIEnv*, jobject,
                                                                           jint keyCode, jboolean down) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->input().keyEvent(keyCode, down == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL Java_com_nebula_engine_NebulaNative_nativeRegisterTexture(JNIEnv* env, jobject,
                                                                                     jstring name,
                                                                                     jbyteArray rgba,
                                                                                     jint width, jint height) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return JNI_FALSE;
    const std::string texName = jstringToStd(env, name);
    const jsize len = env->GetArrayLength(rgba);
    const jsize expected = static_cast<jsize>(width) * height * 4;
    if (len != expected) {
        NEBULA_LOG_E("Texture '%s': byte count %d != %d", texName.c_str(), len, expected);
        return JNI_FALSE;
    }
    std::vector<uint8_t> pixels(static_cast<size_t>(len));
    env->GetByteArrayRegion(rgba, 0, len, reinterpret_cast<jbyte*>(pixels.data()));
    return engine()->registerTexture(texName, pixels.data(), width, height) > 0 ? JNI_TRUE : JNI_FALSE;
}

// ---------------- Scenes ----------------

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeNewScene(JNIEnv* env, jobject, jstring name) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->newScene(jstringToStd(env, name));
}

JNIEXPORT jstring JNICALL Java_com_nebula_engine_NebulaNative_nativeCurrentSceneName(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return stdToJstring(env, engine() ? engine()->currentSceneName() : std::string(""));
}

JNIEXPORT jstring JNICALL Java_com_nebula_engine_NebulaNative_nativeSaveScene(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return stdToJstring(env, engine() ? engine()->saveSceneJson() : std::string("{}"));
}

JNIEXPORT jboolean JNICALL Java_com_nebula_engine_NebulaNative_nativeLoadScene(JNIEnv* env, jobject, jstring jsonText) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return JNI_FALSE;
    return engine()->loadSceneJson(jstringToStd(env, jsonText)) ? JNI_TRUE : JNI_FALSE;
}

// ---------------- Entities ----------------

JNIEXPORT jintArray JNICALL Java_com_nebula_engine_NebulaNative_nativeEntityIds(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    std::vector<Entity> ids = engine() ? engine()->entityIds() : std::vector<Entity>{};
    std::vector<jint> out(ids.begin(), ids.end());
    jintArray arr = env->NewIntArray(static_cast<jsize>(out.size()));
    if (!out.empty()) env->SetIntArrayRegion(arr, 0, static_cast<jsize>(out.size()), out.data());
    return arr;
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeCreateSprite(JNIEnv* env, jobject,
                                                                               jstring texture, jstring name,
                                                                               jfloat x, jfloat y,
                                                                               jfloat w, jfloat h) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return 0;
    return static_cast<jint>(engine()->createSpriteEntity(jstringToStd(env, texture), jstringToStd(env, name),
                                                          Vec2(x, y), Vec2(w, h), 0.0f));
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeDeleteEntity(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->destroyEntity(checkedEntity(id));
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeDuplicateEntity(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return engine() ? static_cast<jint>(engine()->duplicateEntity(checkedEntity(id))) : 0;
}

JNIEXPORT jstring JNICALL Java_com_nebula_engine_NebulaNative_nativeGetEntityName(JNIEnv* env, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return stdToJstring(env, engine() ? engine()->entityName(checkedEntity(id)) : std::string());
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetEntityName(JNIEnv* env, jobject, jint id, jstring name) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setEntityName(checkedEntity(id), jstringToStd(env, name));
}

JNIEXPORT jstring JNICALL Java_com_nebula_engine_NebulaNative_nativeGetEntityTag(JNIEnv* env, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return stdToJstring(env, engine() ? engine()->entityTag(checkedEntity(id)) : std::string());
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetEntityTag(JNIEnv* env, jobject, jint id, jstring tag) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setEntityTag(checkedEntity(id), jstringToStd(env, tag));
}

JNIEXPORT jfloatArray JNICALL Java_com_nebula_engine_NebulaNative_nativeGetTransform(JNIEnv* env, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    float values[5] = {0, 0, 0, 1, 1};
    if (engine()) {
        Vec2 pos, scale; float rot = 0.0f;
        if (engine()->getTransform(checkedEntity(id), pos, rot, scale)) {
            values[0] = pos.x; values[1] = pos.y; values[2] = rot;
            values[3] = scale.x; values[4] = scale.y;
        }
    }
    jfloatArray arr = env->NewFloatArray(5);
    env->SetFloatArrayRegion(arr, 0, 5, values);
    return arr;
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetTransform(JNIEnv*, jobject, jint id,
                                                                               jfloat x, jfloat y,
                                                                               jfloat rotation,
                                                                               jfloat scaleX, jfloat scaleY) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setTransform(checkedEntity(id), Vec2(x, y), rotation, Vec2(scaleX, scaleY));
}

JNIEXPORT jfloatArray JNICALL Java_com_nebula_engine_NebulaNative_nativeGetSpriteInfo(JNIEnv* env, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    float values[7] = {96, 96, 1, 1, 1, 1, 0};
    if (engine()) {
        std::string tex; Vec2 size; Color tint; float z = 0.0f, rot;
        if (engine()->getSprite(checkedEntity(id), tex, size, tint, z, rot)) {
            values[0] = size.x; values[1] = size.y;
            values[2] = tint.r; values[3] = tint.g; values[4] = tint.b; values[5] = tint.a;
            values[6] = z;
        }
    }
    jfloatArray arr = env->NewFloatArray(7);
    env->SetFloatArrayRegion(arr, 0, 7, values);
    return arr;
}

JNIEXPORT jstring JNICALL Java_com_nebula_engine_NebulaNative_nativeGetSpriteTexture(JNIEnv* env, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    std::string tex; Vec2 size; Color tint; float z = 0.0f, rot;
    if (engine() && engine()->getSprite(checkedEntity(id), tex, size, tint, z, rot)) {
        return stdToJstring(env, tex);
    }
    return stdToJstring(env, std::string());
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetSpriteTexture(JNIEnv* env, jobject, jint id, jstring texture) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setSpriteTexture(checkedEntity(id), jstringToStd(env, texture));
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetSpriteSize(JNIEnv*, jobject, jint id, jfloat w, jfloat h) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setSpriteSize(checkedEntity(id), Vec2(w, h));
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetSpriteTint(JNIEnv*, jobject, jint id,
                                                                                jfloat r, jfloat g, jfloat b, jfloat a) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setSpriteTint(checkedEntity(id), Color(r, g, b, a));
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetSpriteZOrder(JNIEnv*, jobject, jint id, jfloat z) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setSpriteZOrder(checkedEntity(id), z);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeAddPhysics(JNIEnv*, jobject, jint id, jboolean isStatic) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return;
    const Entity e = checkedEntity(id);
    engine()->addRigidbody(e, isStatic == JNI_TRUE, 1.0f);
    // Size the box collider to half the sprite bounds (sensible default).
    std::string tex; Vec2 size; Color tint; float z = 0.0f, rot;
    Vec2 half(32.0f, 32.0f);
    if (engine()->getSprite(e, tex, size, tint, z, rot)) half = size * 0.5f;
    engine()->addBoxCollider(e, half);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeRemovePhysics(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->removePhysics(checkedEntity(id));
}

JNIEXPORT jboolean JNICALL Java_com_nebula_engine_NebulaNative_nativeHasPhysics(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return JNI_FALSE;
    return engine()->registry().has<RigidbodyComponent>(checkedEntity(id)) ? JNI_TRUE : JNI_FALSE;
}

// ---------------- Editor camera / picking ----------------

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativePick(JNIEnv*, jobject, jfloat worldX, jfloat worldY) {
    std::lock_guard<std::mutex> lock(gMutex);
    return engine() ? static_cast<jint>(engine()->pick(Vec2(worldX, worldY))) : 0;
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeSelectedEntity(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return engine() ? static_cast<jint>(engine()->selectedEntity()) : 0;
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSelectEntity(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->selectEntity(checkedEntity(id));
}

JNIEXPORT jfloatArray JNICALL Java_com_nebula_engine_NebulaNative_nativeScreenToWorld(JNIEnv* env, jobject, jfloat sx, jfloat sy) {
    std::lock_guard<std::mutex> lock(gMutex);
    Vec2 w = engine() ? engine()->screenToWorld(Vec2(sx, sy)) : Vec2::zero();
    float values[2] = {w.x, w.y};
    jfloatArray arr = env->NewFloatArray(2);
    env->SetFloatArrayRegion(arr, 0, 2, values);
    return arr;
}

JNIEXPORT jfloatArray JNICALL Java_com_nebula_engine_NebulaNative_nativeWorldToScreen(JNIEnv* env, jobject, jfloat wx, jfloat wy) {
    std::lock_guard<std::mutex> lock(gMutex);
    Vec2 s = engine() ? engine()->worldToScreen(Vec2(wx, wy)) : Vec2::zero();
    float values[2] = {s.x, s.y};
    jfloatArray arr = env->NewFloatArray(2);
    env->SetFloatArrayRegion(arr, 0, 2, values);
    return arr;
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativePanCamera(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->panCamera(Vec2(dx, dy));
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeZoomCamera(JNIEnv*, jobject, jfloat factor) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->zoomCamera(factor);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetCamera(JNIEnv*, jobject,
                                                                            jfloat x, jfloat y, jfloat zoom) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!engine()) return;
    engine()->setCameraPosition(Vec2(x, y));
    engine()->setCameraZoom(zoom);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetEditorMode(JNIEnv*, jobject, jboolean enabled) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setEditorMode(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeSetDebugDraw(JNIEnv*, jobject, jboolean enabled) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (engine()) engine()->setDebugDraw(enabled == JNI_TRUE);
}

JNIEXPORT jfloatArray JNICALL Java_com_nebula_engine_NebulaNative_nativeGetStats(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    float values[4] = {0, 0, 0, 0};
    if (engine()) {
        const EngineStats s = engine()->stats();
        values[0] = s.fps;
        values[1] = static_cast<float>(s.drawCalls);
        values[2] = static_cast<float>(s.entityCount);
        values[3] = static_cast<float>(s.particleCount);
    }
    jfloatArray arr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(arr, 0, 4, values);
    return arr;
}

// ---------------- Nebula Run game ----------------

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeStartDemoGame(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gGame) gGame->startDemoScene();
}

JNIEXPORT void JNICALL Java_com_nebula_engine_NebulaNative_nativeStartSandbox(JNIEnv* env, jobject, jstring sceneJson) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gGame) gGame->startSandbox(jstringToStd(env, sceneJson));
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeGameMode(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gGame ? static_cast<jint>(gGame->mode()) : 0;
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeGameScore(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gGame ? gGame->score() : 0;
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeGameLives(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gGame ? gGame->lives() : 0;
}

JNIEXPORT jboolean JNICALL Java_com_nebula_engine_NebulaNative_nativeGameIsOver(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return (gGame && gGame->isGameOver()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_nebula_engine_NebulaNative_nativeGameConsumeSounds(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gGame ? gGame->consumeSoundFlags() : 0;
}

} // extern "C"
