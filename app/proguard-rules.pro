# Keep JNI native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

-keep class com.readingmusic.app.audio.NativeAudioEngine { *; }
