# Nebula Engine — R8/ProGuard rules (release builds)
# Keep rules are intentionally narrow: only names the framework or the native
# side look up indirectly. Everything else is allowed to shrink.

# --- JNI bridge ---------------------------------------------------------------
# Every `external fun` in com.nebula.engine.NebulaNative is invoked from C++
# through JNI (env->FindClass/GetMethodID and direct native call). Renaming
# these would break the bridge at runtime.
-keepclasseswithmembernames class com.nebula.engine.NebulaNative {
    native <methods>;
}
-keep class com.nebula.engine.NebulaNative { *; }

# --- OpenGL renderer ----------------------------------------------------------
# GLSurfaceView instantiates the Renderer implementation reflectively by name
# in some OEMs' instrumentation hooks; keep the class and its ctor.
-keep class * implements android.opengl.GLSurfaceView$Renderer {
    public <init>();
}
-keepnames class * extends android.app.Activity

# --- RecyclerView editor list -------------------------------------------------
# Adapter + ViewHolder are constructed from code (no reflection), but keep the
# ViewHolder constructor signatures for layout inflation robustness.
-keep class com.nebula.engine.editor.EntityListAdapter$ViewHolder {
    public <init>(...);
}

# --- Kotlin metadata -----------------------------------------------------------
-keepattributes *Annotation*, InnerClasses, Signature,
                RuntimeVisibleParameterAnnotations,
                RuntimeInvisibleParameterAnnotations
-dontwarn kotlin.**
-dontwarn org.jetbrains.annotations.**
