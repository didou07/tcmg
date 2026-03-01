package com.tcmg.app;

/**
 * JNI bridge to the native TCMG library.
 * All methods are thread-safe (synchronisation handled in C).
 */
public class TcmgNative {

    static {
        System.loadLibrary("tcmg_jni");
    }

    /**
     * Start the tcmg server in a native background thread.
     *
     * @param cfgDir   absolute path to the config directory (null = built-in default)
     * @param debugLvl debug bit-mask / level 0-9
     * @return 0 on success, negative errno on error
     */
    public static native int startServer(String cfgDir, int debugLvl);

    /** Send a graceful stop signal to the running server. */
    public static native void stopServer();

    /** Returns {@code true} while the server thread is alive. */
    public static native boolean isRunning();

    /**
     * Retrieve log lines accumulated since {@code fromId}.
     *
     * @param fromId    serial id of the first line to return (0 = latest batch)
     * @param maxLines  maximum number of lines to return
     * @param outNextId single-element array; filled with the next serial id to
     *                  pass on the subsequent call
     * @return newline-separated log text (may be empty string, never null)
     */
    public static native String getLogLines(int fromId, int maxLines, int[] outNextId);

    /**
     * Returns the configured WebIF port (e.g. 8080), or -1 if not available.
     */
    public static native int getWebifPort();
}
