# The engine calls these through JNI by name
-keep class app.typemotion.engine.NativeEngine { *; }
-keep interface app.typemotion.engine.NativeCallbacks { *; }
-keep class * implements app.typemotion.engine.NativeCallbacks { *; }
