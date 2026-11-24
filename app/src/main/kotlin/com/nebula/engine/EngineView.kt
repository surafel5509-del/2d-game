package com.nebula.engine

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.view.MotionEvent
import java.io.IOException
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * Nebula Engine viewport.
 *
 * Owns the GL thread: surface creation registers the engine's built-in
 * textures (nebula background, sprites, sparks), the frame loop ticks the
 * C++ engine and presents. Touch events are forwarded to the engine's
 * input state; gesture interpretation (editor camera / drag-move) lives
 * in EditorActivity via the onTouch callback.
 */
class EngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs) {

    /** Called once the GL context + textures are ready (on the GL thread). */
    var onTexturesLoaded: (() -> Unit)? = null

    /** Raw touch events: (action, pointerIndex, xPx, yPx). Return true to consume. */
    var onTouch: ((action: Int, pointerIndex: Int, x: Float, y: Float) -> Boolean)? = null

    private val textures = LinkedHashMap<String, String>()   // name -> asset path
    @Volatile private var texturesReady = false

    init {
        setEGLContextClientVersion(3)
        setRenderer(EngineRenderer())
        renderMode = RENDERMODE_CONTINUOUSLY
        // Engine textures (procedural, shipped in assets/)
        textures["nebula_bg"] = "textures/nebula_bg.png"
        textures["ship"] = "textures/ship.png"
        textures["enemy"] = "textures/enemy.png"
        textures["bullet"] = "textures/bullet.png"
        textures["star"] = "textures/star.png"
    }

    fun areTexturesReady(): Boolean = texturesReady

    fun shutdownEngine() {
        // Called from the UI thread before teardown.
        queueEvent { NebulaNative.nativeShutdown() }
    }

    private fun loadAssetTexture(name: String, assetPath: String) {
        try {
            context.assets.open(assetPath).use { stream ->
                val bitmap = BitmapFactory.decodeStream(stream) ?: return
                val rgba = bitmap.toRgbaBytes()
                NebulaNative.nativeRegisterTexture(name, rgba, bitmap.width, bitmap.height)
                bitmap.recycle()
            }
        } catch (e: IOException) {
            android.util.Log.e("EngineView", "Failed to load texture $name from $assetPath", e)
        }
    }

    private fun Bitmap.toRgbaBytes(): ByteArray {
        val w = width
        val h = height
        val pixels = IntArray(w * h)
        getPixels(pixels, 0, w, 0, 0, w, h)
        val bytes = ByteArray(w * h * 4)
        var i = 0
        for (pixel in pixels) {
            bytes[i++] = ((pixel shr 16) and 0xFF).toByte()  // R
            bytes[i++] = ((pixel shr 8) and 0xFF).toByte()   // G
            bytes[i++] = (pixel and 0xFF).toByte()           // B
            bytes[i++] = ((pixel ushr 24) and 0xFF).toByte() // A
        }
        return bytes
    }

    private inner class EngineRenderer : GLSurfaceView.Renderer {

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            NebulaNative.nativeInit()
            NebulaNative.nativeSurfaceCreated(width.coerceAtLeast(1), height.coerceAtLeast(1))
            for ((name, path) in textures) loadAssetTexture(name, path)
            texturesReady = true
            onTexturesLoaded?.invoke()
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            NebulaNative.nativeSurfaceChanged(width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            NebulaNative.nativeDrawFrame()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val listener = onTouch
        if (listener != null) {
            val action = when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> NebulaNative.TOUCH_DOWN
                MotionEvent.ACTION_MOVE -> NebulaNative.TOUCH_MOVE
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> NebulaNative.TOUCH_UP
                MotionEvent.ACTION_CANCEL -> NebulaNative.TOUCH_CANCEL
                else -> -1
            }
            if (action >= 0) {
                val pointerIndex = event.actionIndex.coerceIn(0, 9)
                // For ACTION_MOVE, report every active pointer.
                var consumed = false
                if (action == NebulaNative.TOUCH_MOVE) {
                    for (i in 0 until event.pointerCount) {
                        consumed = listener(action, i.coerceAtMost(9), event.getX(i), event.getY(i)) || consumed
                    }
                } else {
                    val x = event.getX(event.actionIndex)
                    val y = event.getY(event.actionIndex)
                    NebulaNative.nativeTouchEvent(pointerIndex, action, x, y)
                    consumed = listener(action, pointerIndex, x, y) || consumed
                }
                if (consumed) return true
            }
            return true
        }
        // Default: forward to the engine input state (demo gameplay).
        val action = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> NebulaNative.TOUCH_DOWN
            MotionEvent.ACTION_MOVE -> NebulaNative.TOUCH_MOVE
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> NebulaNative.TOUCH_UP
            MotionEvent.ACTION_CANCEL -> NebulaNative.TOUCH_CANCEL
            else -> return super.onTouchEvent(event)
        }
        if (action == NebulaNative.TOUCH_MOVE) {
            for (i in 0 until event.pointerCount) {
                NebulaNative.nativeTouchEvent(i.coerceAtMost(9), action, event.getX(i), event.getY(i))
            }
        } else {
            val idx = event.actionIndex.coerceIn(0, 9)
            NebulaNative.nativeTouchEvent(idx, action, event.getX(idx), event.getY(idx))
        }
        return true
    }
}
