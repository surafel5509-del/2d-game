# Nebula Engine — keep JNI symbols for the native bridge
-keep class com.nebula.engine.NebulaNative { *; }
-keepnames class com.nebula.engine.** { native <methods>; }
