package com.nebula.engine.game

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.nebula.engine.EngineView
import com.nebula.engine.NebulaNative
import com.nebula.engine.R
import com.nebula.engine.audio.SoundManager

/**
 * Nebula Run — flagship demo, or a live playtest of an editor scene
 * when launched with the "scene_json" intent extra.
 */
class GameActivity : AppCompatActivity() {

    private lateinit var engineView: EngineView
    private lateinit var sound: SoundManager

    private lateinit var tvScore: TextView
    private lateinit var tvLives: TextView
    private lateinit var tvHint: TextView
    private lateinit var tvSandboxBanner: TextView
    private lateinit var panelScore: View
    private lateinit var panelLives: View
    private lateinit var panelGameOver: View
    private lateinit var tvFinalScore: TextView
    private lateinit var tvBestScore: TextView

    private val ui = Handler(Looper.getMainLooper())
    private var isSandbox = false
    private var gameOverShown = false

    private val hudTicker = object : Runnable {
        override fun run() {
            refreshHud()
            ui.postDelayed(this, 80)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_game)

        NebulaNative.nativeInit()
        sound = SoundManager(this)

        engineView = findViewById(R.id.engineView)
        tvScore = findViewById(R.id.tvScore)
        tvLives = findViewById(R.id.tvLives)
        tvHint = findViewById(R.id.tvHint)
        tvSandboxBanner = findViewById(R.id.tvSandboxBanner)
        panelScore = findViewById(R.id.panelScore)
        panelLives = findViewById(R.id.panelLives)
        panelGameOver = findViewById(R.id.panelGameOver)
        tvFinalScore = findViewById(R.id.tvFinalScore)
        tvBestScore = findViewById(R.id.tvBestScore)

        val sandboxJson = intent.getStringExtra(EXTRA_SCENE_JSON)
        isSandbox = !sandboxJson.isNullOrBlank()

        if (isSandbox) {
            panelScore.visibility = View.GONE
            panelLives.visibility = View.GONE
            tvSandboxBanner.visibility = View.VISIBLE
            tvHint.visibility = View.GONE
        }

        engineView.onTexturesLoaded = {
            engineView.queueEvent {
                NebulaNative.nativeSetEditorMode(false)
                NebulaNative.nativeSetDebugDraw(false)
                if (isSandbox) NebulaNative.nativeStartSandbox(sandboxJson!!)
                else NebulaNative.nativeStartDemoGame()
            }
        }

        findViewById<View>(R.id.btnExit).setOnClickListener {
            sound.playClick()
            finish()
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
        }
        findViewById<com.google.android.material.button.MaterialButton>(R.id.btnRetry).setOnClickListener {
            sound.playClick()
            gameOverShown = false
            panelGameOver.visibility = View.GONE
            engineView.queueEvent { NebulaNative.nativeStartDemoGame() }
        }
        findViewById<com.google.android.material.button.MaterialButton>(R.id.btnMainMenu).setOnClickListener {
            sound.playClick()
            finish()
        }

        // Auto-hide the steering hint.
        ui.postDelayed({ tvHint.animate().alpha(0f).setDuration(450).withEndAction { tvHint.visibility = View.GONE }.start() }, 3600)
    }

    private fun refreshHud() {
        // Sound flags are produced inside the C++ game each frame.
        val flags = NebulaNative.nativeGameConsumeSounds()
        if (flags and NebulaNative.SOUND_LASER != 0) sound.playLaser()
        if (flags and NebulaNative.SOUND_EXPLOSION != 0) sound.playExplosion()
        if (flags and NebulaNative.SOUND_HIT != 0) sound.playHit()
        if (flags and NebulaNative.SOUND_GAMEOVER != 0) {
            sound.playGameOver()
            showGameOver()
        }

        if (isSandbox) return

        tvScore.text = NebulaNative.nativeGameScore().toString()
        val lives = NebulaNative.nativeGameLives().coerceIn(0, 3)
        tvLives.text = "❤".repeat(lives) + "♡".repeat(3 - lives)

        if (!gameOverShown && NebulaNative.nativeGameIsOver()) showGameOver()
    }

    private fun showGameOver() {
        if (gameOverShown || isSandbox) return
        gameOverShown = true

        val finalScore = NebulaNative.nativeGameScore()
        val prefs = getSharedPreferences("nebula_best", MODE_PRIVATE)
        val best = maxOf(prefs.getInt(KEY_BEST, 0), finalScore)
        prefs.edit().putInt(KEY_BEST, best).apply()

        tvFinalScore.text = getString(R.string.hud_score) + "  " + finalScore
        tvBestScore.text = getString(R.string.hud_best) + "  " + best
        panelGameOver.visibility = View.VISIBLE
        panelGameOver.alpha = 0f
        panelGameOver.animate().alpha(1f).setDuration(420).start()
    }

    override fun onResume() {
        super.onResume()
        ui.post(hudTicker)
    }

    override fun onPause() {
        super.onPause()
        ui.removeCallbacks(hudTicker)
    }

    override fun onDestroy() {
        sound.release()
        super.onDestroy()
    }

    companion object {
        const val EXTRA_SCENE_JSON = "scene_json"
        private const val KEY_BEST = "best_score"
    }
}
