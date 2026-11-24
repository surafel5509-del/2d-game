package com.nebula.engine

import android.content.Intent
import android.os.Bundle
import android.view.animation.AlphaAnimation
import android.view.animation.AnimationSet
import android.view.animation.DecelerateInterpolator
import android.view.animation.TranslateAnimation
import android.widget.ImageView
import androidx.appcompat.app.AppCompatActivity
import com.nebula.engine.audio.SoundManager
import com.nebula.engine.editor.EditorActivity
import com.nebula.engine.game.GameActivity

/**
 * Nebula Engine hub. Choose between the flagship demo
 * (Nebula Run) or the on-device 2D scene editor.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var sound: SoundManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        NebulaNative.nativeInit()
        sound = SoundManager(this)

        // Entrance animation: logo drifts down & fades in.
        findViewById<ImageView>(R.id.ivLogo).startAnimation(
            AnimationSet(true).apply {
                addAnimation(AlphaAnimation(0f, 1f).apply { duration = 650 })
                addAnimation(
                    TranslateAnimation(0f, 0f, -120f, 0f).apply {
                        duration = 750
                        interpolator = DecelerateInterpolator()
                    }
                )
            }
        )

        findViewById<android.view.View>(R.id.cardGame).setOnClickListener { launchGame() }
        findViewById<com.google.android.material.button.MaterialButton>(R.id.btnPlayGame).setOnClickListener { launchGame() }
        findViewById<android.view.View>(R.id.cardEditor).setOnClickListener { launchEditor() }
        findViewById<com.google.android.material.button.MaterialButton>(R.id.btnOpenEditor).setOnClickListener { launchEditor() }
    }

    private fun launchGame() {
        sound.playClick()
        startActivity(Intent(this, GameActivity::class.java))
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
    }

    private fun launchEditor() {
        sound.playClick()
        startActivity(Intent(this, EditorActivity::class.java))
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
    }

    override fun onDestroy() {
        sound.release()
        super.onDestroy()
    }
}
