package com.nebula.engine

/**
 * Nebula Engine — native bridge.
 * Every engine-facing call crosses JNI into C++17 and is serialized
 * through one mutex inside the native layer. Safe to call from any thread.
 */
object NebulaNative {

    init {
        System.loadLibrary("nebula2d")
    }

    // ---- Lifecycle ----
    external fun nativeInit(): Boolean
    external fun nativeShutdown()
    external fun nativeSurfaceCreated(width: Int, height: Int)
    external fun nativeSurfaceChanged(width: Int, height: Int)
    external fun nativeDrawFrame()

    // ---- Input (screen-pixel coordinates) ----
    external fun nativeTouchEvent(pointerIndex: Int, action: Int, x: Float, y: Float)
    external fun nativeKeyEvent(keyCode: Int, down: Boolean)

    // ---- Textures ----
    external fun nativeRegisterTexture(name: String, rgba: ByteArray, width: Int, height: Int): Boolean

    // ---- Scenes ----
    external fun nativeNewScene(name: String)
    external fun nativeCurrentSceneName(): String
    external fun nativeSaveScene(): String
    external fun nativeLoadScene(jsonText: String): Boolean

    // ---- Entities ----
    external fun nativeEntityIds(): IntArray
    external fun nativeCreateSprite(texture: String, name: String, x: Float, y: Float, w: Float, h: Float): Int
    external fun nativeDeleteEntity(id: Int)
    external fun nativeDuplicateEntity(id: Int): Int
    external fun nativeGetEntityName(id: Int): String
    external fun nativeSetEntityName(id: Int, name: String)
    external fun nativeGetEntityTag(id: Int): String
    external fun nativeSetEntityTag(id: Int, tag: String)
    external fun nativeGetTransform(id: Int): FloatArray      // [x, y, rot(rad), scaleX, scaleY]
    external fun nativeSetTransform(id: Int, x: Float, y: Float, rotation: Float, scaleX: Float, scaleY: Float)
    external fun nativeGetSpriteInfo(id: Int): FloatArray     // [sizeW, sizeH, tintR, tintG, tintB, tintA, zOrder]
    external fun nativeGetSpriteTexture(id: Int): String
    external fun nativeSetSpriteTexture(id: Int, texture: String)
    external fun nativeSetSpriteSize(id: Int, w: Float, h: Float)
    external fun nativeSetSpriteTint(id: Int, r: Float, g: Float, b: Float, a: Float)
    external fun nativeSetSpriteZOrder(id: Int, z: Float)
    external fun nativeAddPhysics(id: Int, isStatic: Boolean)
    external fun nativeRemovePhysics(id: Int)
    external fun nativeHasPhysics(id: Int): Boolean

    // ---- Editor camera & picking ----
    external fun nativePick(worldX: Float, worldY: Float): Int
    external fun nativeSelectedEntity(): Int
    external fun nativeSelectEntity(id: Int)
    external fun nativeScreenToWorld(sx: Float, sy: Float): FloatArray  // [x, y]
    external fun nativeWorldToScreen(wx: Float, wy: Float): FloatArray
    external fun nativePanCamera(dx: Float, dy: Float)
    external fun nativeZoomCamera(factor: Float)
    external fun nativeSetCamera(x: Float, y: Float, zoom: Float)
    external fun nativeSetEditorMode(enabled: Boolean)
    external fun nativeSetDebugDraw(enabled: Boolean)
    external fun nativeGetStats(): FloatArray                  // [fps, drawCalls, entities, particles]

    // ---- Nebula Run / sandbox playtest ----
    external fun nativeStartDemoGame()
    external fun nativeStartSandbox(sceneJson: String)
    external fun nativeGameMode(): Int                        // 0 = demo, 1 = sandbox
    external fun nativeGameScore(): Int
    external fun nativeGameLives(): Int
    external fun nativeGameIsOver(): Boolean
    external fun nativeGameConsumeSounds(): Int               // bit0 laser, bit1 explosion, bit2 hit, bit3 gameover

    const val TOUCH_DOWN = 0
    const val TOUCH_MOVE = 1
    const val TOUCH_UP = 2
    const val TOUCH_CANCEL = 3

    const val SOUND_LASER = 1
    const val SOUND_EXPLOSION = 2
    const val SOUND_HIT = 4
    const val SOUND_GAMEOVER = 8
}
