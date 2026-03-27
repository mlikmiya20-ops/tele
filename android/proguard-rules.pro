# TELECLOUD-MULTI ProGuard Configuration
# Optimizes APK size while preserving required functionality

# Keep Qt classes
-keep class org.qtproject.qt.** { *; }
-keep class org.kde.necessitas.** { *; }

# Keep FFmpeg native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep all native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep JNI methods
-keep class * {
    native <methods>;
}

# Keep Java classes called from C++
-keep class com.telecloud.multi.** { *; }

# Keep parcelable classes
-keep class * implements android.os.Parcelable {
    public static final android.os.Parcelable$Creator *;
}

# Keep serializable classes
-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    !static !transient <fields>;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}

# Keep enums
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# Remove logging in release builds
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int i(...);
    public static int w(...);
    public static int d(...);
}

# Optimize
-optimizationpasses 5
-dontusemixedcaseclassnames
-dontskipnonpubliclibraryclasses
-verbose

# Optimization settings
-optimizations !code/simplification/arithmetic,!field/*,!class/merging/*,!code/allocation/variable

# Allow optimization
-allowaccessmodification

# Keep annotation classes
-keepattributes *Annotation*

# Keep debugging info for crash reports
-keepattributes SourceFile,LineNumberTable

# Rename source file names for better obfuscation
-renamesourcefileattribute SourceFile

# Warning suppression
-dontwarn org.qtproject.qt.**
-dontwarn javax.**
-dontwarn java.**
