# ── TCMG native bridge ────────────────────────────────────────────────────────
-keep class com.tcmg.app.TcmgNative {
    public static native <methods>;
}

# ── Activity / Fragment / Service / Receiver ─────────────────────────────────
-keep public class com.tcmg.app.MainActivity
-keep public class com.tcmg.app.ControlFragment
-keep public class com.tcmg.app.LogFragment
-keep public class com.tcmg.app.EditorFragment
-keep public class com.tcmg.app.TcmgService
-keep public class com.tcmg.app.BootReceiver

# ── ViewBinding ───────────────────────────────────────────────────────────────
-keep class com.tcmg.app.databinding.** { *; }

# ── AndroidX / Material ───────────────────────────────────────────────────────
-dontwarn androidx.**
-keep class androidx.** { *; }
-keep interface androidx.** { *; }
-keep class com.google.android.material.** { *; }

# ── Parcelables ───────────────────────────────────────────────────────────────
-keepclassmembers class * implements android.os.Parcelable {
    public static final ** CREATOR;
}

# ── Enums ─────────────────────────────────────────────────────────────────────
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# ── Strip debug logs in release ───────────────────────────────────────────────
-assumenosideeffects class android.util.Log {
    public static int v(...);
    public static int d(...);
}
