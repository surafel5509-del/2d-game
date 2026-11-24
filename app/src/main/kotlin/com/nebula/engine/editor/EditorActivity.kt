package com.nebula.engine.editor

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.EditText
import android.widget.ImageButton
import android.widget.PopupMenu
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import com.nebula.engine.EngineView
import com.nebula.engine.NebulaNative
import com.nebula.engine.R
import com.nebula.engine.audio.SoundManager
import com.nebula.engine.game.GameActivity
import java.io.File
import kotlin.math.hypot

/**
 * Nebula Engine — on-device 2D scene editor.
 *
 * Workspace: hierarchy (left) · live GLES viewport with pick & drag-move,
 * pinch-zoom and two-finger pan (center) · inspector (right) with transform,
 * tint and physics editing. Scenes serialize to portable .nebula.json files.
 */
class EditorActivity : AppCompatActivity() {

    private lateinit var engineView: EngineView
    private lateinit var sound: SoundManager
    private lateinit var hierarchy: RecyclerView
    private lateinit var adapter: EntityListAdapter

    private lateinit var tvNoSelection: TextView
    private lateinit var inspectorBody: View
    private lateinit var etName: EditText
    private lateinit var etTag: EditText
    private lateinit var etPosX: EditText
    private lateinit var etPosY: EditText
    private lateinit var etScaleX: EditText
    private lateinit var etScaleY: EditText
    private lateinit var etRotation: EditText
    private lateinit var etSizeW: EditText
    private lateinit var etSizeH: EditText
    private lateinit var etZOrder: EditText
    private lateinit var sbTintR: SeekBar
    private lateinit var sbTintG: SeekBar
    private lateinit var sbTintB: SeekBar
    private lateinit var sbTintA: SeekBar
    private lateinit var tvStatus: TextView
    private lateinit var switchDebug: Switch

    private val ui = Handler(Looper.getMainLooper())
    private var selectedId: Int = 0
    private var loadingInspector = false

    // Viewport gesture state
    private var dragEntity = false
    private var panning = false
    private var grabOffsetX = 0f
    private var grabOffsetY = 0f
    private var lastPanX = 0f
    private var lastPanY = 0f
    private val activePointers = HashMap<Int, Pair<Float, Float>>()
    private var pinchDistance = 0f
    private var pinchCentroidX = 0f
    private var pinchCentroidY = 0f

    private val statusTicker = object : Runnable {
        override fun run() {
            val stats = NebulaNative.nativeGetStats()
            tvStatus.text = "FPS %.0f  ·  Draws %.0f  ·  Entities %.0f  ·  Particles %.0f"
                .format(stats[0], stats[1], stats[2], stats[3])
            ui.postDelayed(this, 400)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_editor)

        NebulaNative.nativeInit()
        sound = SoundManager(this)
        bindViews()
        setupHierarchy()
        setupToolbar()
        setupInspector()
        setupViewport()

        engineView.onTexturesLoaded = {
            engineView.queueEvent {
                NebulaNative.nativeSetEditorMode(true)
                // Fit ~800 logical units across the viewport width.
                val zoom = (engineView.width.coerceAtLeast(1)) / 800f
                NebulaNative.nativeSetCamera(0f, 0f, zoom)
                seedStarterSceneIfEmpty()
            }
            ui.post { refreshHierarchy() }
        }
    }

    // ────────────────────────────────────────────────────────
    // View setup
    // ────────────────────────────────────────────────────────

    private fun bindViews() {
        engineView = findViewById(R.id.editorViewport)
        hierarchy = findViewById(R.id.rvHierarchy)
        tvNoSelection = findViewById(R.id.tvNoSelection)
        inspectorBody = findViewById(R.id.inspectorBody)
        etName = findViewById(R.id.etName)
        etTag = findViewById(R.id.etTag)
        etPosX = findViewById(R.id.etPosX)
        etPosY = findViewById(R.id.etPosY)
        etScaleX = findViewById(R.id.etScaleX)
        etScaleY = findViewById(R.id.etScaleY)
        etRotation = findViewById(R.id.etRotation)
        etSizeW = findViewById(R.id.etSizeW)
        etSizeH = findViewById(R.id.etSizeH)
        etZOrder = findViewById(R.id.etZOrder)
        sbTintR = findViewById(R.id.sbTintR)
        sbTintG = findViewById(R.id.sbTintG)
        sbTintB = findViewById(R.id.sbTintB)
        sbTintA = findViewById(R.id.sbTintA)
        tvStatus = findViewById(R.id.tvStatus)
        switchDebug = findViewById(R.id.switchDebug)
    }

    private fun setupHierarchy() {
        adapter = EntityListAdapter { item ->
            sound.playClick()
            selectEntity(item.id)
        }
        hierarchy.layoutManager = LinearLayoutManager(this)
        hierarchy.adapter = adapter
    }

    private fun setupToolbar() {
        findViewById<ImageButton>(R.id.btnEditorBack).setOnClickListener { finish() }
        findViewById<ImageButton>(R.id.btnAddSprite).setOnClickListener { showAddSpriteMenu(it) }
        findViewById<ImageButton>(R.id.btnAddPhysics).setOnClickListener { showAddPhysicsMenu(it) }
        findViewById<ImageButton>(R.id.btnDuplicate).setOnClickListener {
            if (selectedId != 0) {
                val newId = NebulaNative.nativeDuplicateEntity(selectedId)
                if (newId != 0) {
                    sound.playClick()
                    selectEntity(newId)
                }
            }
        }
        findViewById<ImageButton>(R.id.btnDelete).setOnClickListener {
            if (selectedId != 0) {
                NebulaNative.nativeDeleteEntity(selectedId)
                sound.playClick()
                selectEntity(0)
            }
        }
        findViewById<ImageButton>(R.id.btnSave).setOnClickListener { saveScene(showToast = true) }
        findViewById<ImageButton>(R.id.btnLoad).setOnClickListener { loadScene(showToast = true) }
        findViewById<MaterialButton>(R.id.btnPlay).setOnClickListener { playScene() }
        switchDebug.setOnCheckedChangeListener { _, checked ->
            NebulaNative.nativeSetDebugDraw(checked)
        }
    }

    private fun setupInspector() {
        // Name & tag apply on Done / focus loss.
        etName.applyOnDone { applyTextFields() }
        etTag.applyOnDone { applyTextFields() }

        // Numeric fields apply the whole transform block atomically.
        for (field in listOf(etPosX, etPosY, etScaleX, etScaleY, etRotation, etSizeW, etSizeH, etZOrder)) {
            field.applyOnDone { applyNumericFields() }
        }

        val tintChanged = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser || loadingInspector || selectedId == 0) return
                NebulaNative.nativeSetSpriteTint(
                    selectedId,
                    sbTintR.progress / 255f,
                    sbTintG.progress / 255f,
                    sbTintB.progress / 255f,
                    sbTintA.progress / 255f
                )
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }
        sbTintR.setOnSeekBarChangeListener(tintChanged)
        sbTintG.setOnSeekBarChangeListener(tintChanged)
        sbTintB.setOnSeekBarChangeListener(tintChanged)
        sbTintA.setOnSeekBarChangeListener(tintChanged)

        findViewById<MaterialButton>(R.id.btnPhysicsDynamic).setOnClickListener {
            if (selectedId != 0) {
                NebulaNative.nativeAddPhysics(selectedId, isStatic = false)
                sound.playClick()
                toast("Dynamic body + box collider added")
            }
        }
        findViewById<MaterialButton>(R.id.btnPhysicsStatic).setOnClickListener {
            if (selectedId != 0) {
                NebulaNative.nativeAddPhysics(selectedId, isStatic = true)
                sound.playClick()
                toast("Static body + box collider added")
            }
        }
        findViewById<MaterialButton>(R.id.btnRemovePhysics).setOnClickListener {
            if (selectedId != 0) {
                NebulaNative.nativeRemovePhysics(selectedId)
                sound.playClick()
                toast("Physics removed")
            }
        }
    }

    private fun setupViewport() {
        engineView.onTouch = { action, pointerIndex, x, y ->
            handleViewportTouch(action, pointerIndex, x, y)
            true
        }
    }

    // ────────────────────────────────────────────────────────
    // Viewport gestures: tap-pick, drag-move, pan, pinch-zoom
    // ────────────────────────────────────────────────────────

    private fun handleViewportTouch(action: Int, pointerIndex: Int, x: Float, y: Float) {
        when (action) {
            NebulaNative.TOUCH_DOWN -> {
                activePointers[pointerIndex] = x to y
                if (activePointers.size == 1) {
                    val world = screenToWorld(x, y)
                    val picked = NebulaNative.nativePick(world[0], world[1])
                    selectEntity(picked)
                    if (picked != 0) {
                        val t = NebulaNative.nativeGetTransform(picked)
                        dragEntity = true
                        grabOffsetX = t[0] - world[0]
                        grabOffsetY = t[1] - world[1]
                    } else {
                        panning = true
                        lastPanX = x
                        lastPanY = y
                    }
                } else if (activePointers.size == 2) {
                    // Switch from editing gesture to pinch camera control.
                    dragEntity = false
                    panning = false
                    val (p1, p2) = activePointers.values.toList()
                    pinchDistance = hypot(p2.first - p1.first, p2.second - p1.second)
                    pinchCentroidX = (p1.first + p2.first) / 2f
                    pinchCentroidY = (p1.second + p2.second) / 2f
                }
            }

            NebulaNative.TOUCH_MOVE -> {
                if (activePointers.containsKey(pointerIndex)) {
                    activePointers[pointerIndex] = x to y
                }
                when {
                    activePointers.size == 2 -> handlePinch()
                    dragEntity && selectedId != 0 -> {
                        val world = screenToWorld(x, y)
                        val t = NebulaNative.nativeGetTransform(selectedId)
                        NebulaNative.nativeSetTransform(
                            selectedId,
                            world[0] + grabOffsetX,
                            world[1] + grabOffsetY,
                            t[2], t[3], t[4]
                        )
                        loadInspectorTransform()
                    }
                    panning -> {
                        val before = screenToWorld(lastPanX, lastPanY)
                        val now = screenToWorld(x, y)
                        NebulaNative.nativePanCamera(before[0] - now[0], before[1] - now[1])
                        lastPanX = x
                        lastPanY = y
                    }
                }
            }

            NebulaNative.TOUCH_UP, NebulaNative.TOUCH_CANCEL -> {
                activePointers.remove(pointerIndex)
                if (activePointers.size < 2) pinchDistance = 0f
                if (activePointers.isEmpty()) {
                    dragEntity = false
                    panning = false
                } else if (activePointers.size == 1) {
                    // Resume pan/edit with the remaining finger.
                    val remaining = activePointers.values.first()
                    lastPanX = remaining.first
                    lastPanY = remaining.second
                    dragEntity = false
                    panning = true
                }
            }
        }
    }

    private fun handlePinch() {
        val (p1, p2) = activePointers.values.toList()
        val distance = hypot(p2.first - p1.first, p2.second - p1.second)
        val centroidX = (p1.first + p2.first) / 2f
        val centroidY = (p1.second + p2.second) / 2f

        if (pinchDistance > 1f && distance > 1f) {
            val ratio = distance / pinchDistance
            // Keep the pinch focus pinned in world space:
            // remember where it is, zoom, then shift the camera back.
            val focusBefore = screenToWorld(centroidX, centroidY)
            NebulaNative.nativeZoomCamera(ratio)
            val focusAfter = screenToWorld(centroidX, centroidY)
            NebulaNative.nativePanCamera(focusBefore[0] - focusAfter[0], focusBefore[1] - focusAfter[1])
        }

        // Two-finger drag pans as well.
        val prevCentroid = screenToWorld(pinchCentroidX, pinchCentroidY)
        val nowCentroid = screenToWorld(centroidX, centroidY)
        NebulaNative.nativePanCamera(prevCentroid[0] - nowCentroid[0], prevCentroid[1] - nowCentroid[1])

        pinchDistance = distance
        pinchCentroidX = centroidX
        pinchCentroidY = centroidY
    }

    private fun screenToWorld(x: Float, y: Float): FloatArray =
        NebulaNative.nativeScreenToWorld(x, y)

    // ────────────────────────────────────────────────────────
    // Hierarchy & selection
    // ────────────────────────────────────────────────────────

    private fun selectEntity(id: Int) {
        selectedId = id
        NebulaNative.nativeSelectEntity(id)
        refreshHierarchySelection()
        loadInspector()
    }

    private fun refreshHierarchy() {
        val items = NebulaNative.nativeEntityIds().map { id ->
            EntityItem(
                id = id,
                name = NebulaNative.nativeGetEntityName(id).ifEmpty { "Entity #$id" },
                tag = NebulaNative.nativeGetEntityTag(id)
            )
        }
        adapter.selectedId = selectedId
        adapter.submit(items)
    }

    private fun refreshHierarchySelection() {
        adapter.selectedId = selectedId
    }

    // ────────────────────────────────────────────────────────
    // Inspector
    // ────────────────────────────────────────────────────────

    private fun loadInspector() {
        loadingInspector = true
        try {
            val has = selectedId != 0
            tvNoSelection.visibility = if (has) View.GONE else View.VISIBLE
            inspectorBody.visibility = if (has) View.VISIBLE else View.GONE
            if (!has) return

            etName.setText(NebulaNative.nativeGetEntityName(selectedId))
            etTag.setText(NebulaNative.nativeGetEntityTag(selectedId))
            loadInspectorTransform()

            val info = NebulaNative.nativeGetSpriteInfo(selectedId)
            etSizeW.setText(fmt(info[0]))
            etSizeH.setText(fmt(info[1]))
            etZOrder.setText(fmt(info[6]))
            sbTintR.progress = (info[2] * 255f).toInt().coerceIn(0, 255)
            sbTintG.progress = (info[3] * 255f).toInt().coerceIn(0, 255)
            sbTintB.progress = (info[4] * 255f).toInt().coerceIn(0, 255)
            sbTintA.progress = (info[5] * 255f).toInt().coerceIn(0, 255)
        } finally {
            loadingInspector = false
        }
    }

    private fun loadInspectorTransform() {
        val wasLoading = loadingInspector
        loadingInspector = true
        try {
            val t = NebulaNative.nativeGetTransform(selectedId)
            etPosX.setText(fmt(t[0]))
            etPosY.setText(fmt(t[1]))
            etRotation.setText(fmt(Math.toDegrees(t[2].toDouble()).toFloat()))
            etScaleX.setText(fmt(t[3]))
            etScaleY.setText(fmt(t[4]))
        } finally {
            loadingInspector = wasLoading
        }
    }

    private fun applyTextFields() {
        if (loadingInspector || selectedId == 0) return
        NebulaNative.nativeSetEntityName(selectedId, etName.text?.toString().orEmpty().ifEmpty { "Entity" })
        NebulaNative.nativeSetEntityTag(selectedId, etTag.text?.toString().orEmpty().trim())
        refreshHierarchy()
    }

    private fun applyNumericFields() {
        if (loadingInspector || selectedId == 0) return
        val t = NebulaNative.nativeGetTransform(selectedId)
        val posX = etPosX.text.toFloatOr(t[0])
        val posY = etPosY.text.toFloatOr(t[1])
        val rot = Math.toRadians(etRotation.text.toFloatOr(Math.toDegrees(t[2].toDouble()).toFloat()).toDouble()).toFloat()
        val scaleX = etScaleX.text.toFloatOr(t[3])
        val scaleY = etScaleY.text.toFloatOr(t[4])
        NebulaNative.nativeSetTransform(selectedId, posX, posY, rot, scaleX, scaleY)

        val info = NebulaNative.nativeGetSpriteInfo(selectedId)
        val sizeW = etSizeW.text.toFloatOr(info[0]).coerceAtLeast(1f)
        val sizeH = etSizeH.text.toFloatOr(info[1]).coerceAtLeast(1f)
        NebulaNative.nativeSetSpriteSize(selectedId, sizeW, sizeH)
        NebulaNative.nativeSetSpriteZOrder(selectedId, etZOrder.text.toFloatOr(info[6]))
    }

    private fun CharSequence?.toFloatOr(fallback: Float): Float =
        this?.toString()?.trim()?.toFloatOrNull() ?: fallback

    private fun fmt(value: Float): String {
        val rounded = (value * 100f).toInt() / 100f
        return if (rounded == rounded.toLong().toFloat()) rounded.toLong().toString() else rounded.toString()
    }

    // ────────────────────────────────────────────────────────
    // Toolbar actions
    // ────────────────────────────────────────────────────────

    private fun showAddSpriteMenu(anchor: View) {
        val menu = PopupMenu(this, anchor, Gravity.START, 0, R.style.Theme_Nebula)
        val textures = listOf(
            "ship" to "Ship (118px)",
            "enemy" to "Enemy drone (96px)",
            "bullet" to "Energy bolt",
            "star" to "Star (48px)",
            "nebula_bg" to "Nebula backdrop"
        )
        textures.forEachIndexed { index, (key, label) -> menu.menu.add(0, index, index, label) }
        menu.setOnMenuItemClickListener { item ->
            val (texture, _) = textures[item.itemId]
            val center = NebulaNative.nativeScreenToWorld(engineView.width / 2f, engineView.height / 2f)
            val (w, h) = when (texture) {
                "ship" -> 118f to 118f
                "enemy" -> 96f to 96f
                "bullet" -> 16f to 44f
                "star" -> 48f to 48f
                else -> 512f to 512f
            }
            val id = NebulaNative.nativeCreateSprite(texture, texture.replaceFirstChar { it.uppercase() }, center[0], center[1], w, h)
            if (id != 0) {
                sound.playClick()
                selectEntity(id)
                refreshHierarchy()
            }
            true
        }
        menu.show()
    }

    private fun showAddPhysicsMenu(anchor: View) {
        if (selectedId == 0) {
            toast("Select an entity first")
            return
        }
        val menu = PopupMenu(this, anchor, Gravity.START, 0, R.style.Theme_Nebula)
        menu.menu.add(0, 0, 0, "Dynamic (falls, bounces)")
        menu.menu.add(0, 1, 1, "Static (immovable wall)")
        menu.setOnMenuItemClickListener { item ->
            NebulaNative.nativeAddPhysics(selectedId, isStatic = item.itemId == 1)
            sound.playClick()
            toast(if (item.itemId == 1) "Static body added" else "Dynamic body added")
            true
        }
        menu.show()
    }

    private fun saveScene(showToast: Boolean) {
        val json = NebulaNative.nativeSaveScene()
        try {
            File(filesDir, SCENE_FILE).writeText(json)
            if (showToast) toast(getString(R.string.editor_saved))
        } catch (e: Exception) {
            if (showToast) toast(getString(R.string.editor_save_error))
        }
    }

    private fun loadScene(showToast: Boolean): Boolean {
        val file = File(filesDir, SCENE_FILE)
        if (!file.exists()) {
            if (showToast) toast(getString(R.string.editor_load_error))
            return false
        }
        val ok = try {
            NebulaNative.nativeLoadScene(file.readText())
        } catch (e: Exception) {
            false
        }
        if (ok) {
            selectEntity(0)
            refreshHierarchy()
            if (showToast) toast(getString(R.string.editor_loaded))
        } else if (showToast) {
            toast(getString(R.string.editor_load_error))
        }
        return ok
    }

    private fun playScene() {
        // Auto-save, then boot the scene into the live runtime.
        saveScene(showToast = false)
        val json = NebulaNative.nativeSaveScene()
        val intent = Intent(this, GameActivity::class.java)
            .putExtra(GameActivity.EXTRA_SCENE_JSON, json)
        sound.playClick()
        startActivity(intent)
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
    }

    private fun seedStarterSceneIfEmpty() {
        // If the scene is empty, drop in a helpful starter composition.
        if (NebulaNative.nativeEntityIds().isNotEmpty()) return
        val bg = NebulaNative.nativeCreateSprite("nebula_bg", "Backdrop", 0f, 0f, 1024f, 1024f)
        NebulaNative.nativeSetSpriteZOrder(bg, -10f)
        val ship = NebulaNative.nativeCreateSprite("ship", "Player Ship", 0f, -60f, 118f, 118f)
        NebulaNative.nativeSetEntityTag(ship, "player")
        val enemy = NebulaNative.nativeCreateSprite("enemy", "Drone", 140f, 180f, 96f, 96f)
        NebulaNative.nativeSetEntityTag(enemy, "enemy")
        NebulaNative.nativeSelectEntity(ship)
        selectedId = ship
        ui.post { refreshHierarchy() }
    }

    private fun toast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    // ────────────────────────────────────────────────────────
    // Lifecycle
    // ────────────────────────────────────────────────────────

    override fun onResume() {
        super.onResume()
        NebulaNative.nativeSetEditorMode(true)
        ui.post(statusTicker)
        refreshHierarchy()
        loadInspector()
    }

    override fun onPause() {
        super.onPause()
        ui.removeCallbacks(statusTicker)
        // Auto-save the current scene so an unsaved edit is never lost.
        saveScene(showToast = false)
    }

    override fun onDestroy() {
        sound.release()
        super.onDestroy()
    }

    companion object {
        private const val SCENE_FILE = "scene.nebula.json"
    }
}

// ────────────────────────────────────────────────────────
// Small helpers
// ────────────────────────────────────────────────────────

private fun EditText.applyOnDone(action: () -> Unit) {
    setOnFocusChangeListener { _, hasFocus -> if (!hasFocus) action() }
    setOnEditorActionListener { _, _, _ ->
        action()
        clearFocus()
        true
    }
}
