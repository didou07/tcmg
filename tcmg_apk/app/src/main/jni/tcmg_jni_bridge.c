/*
 * tcmg_jni_bridge.c
 *
 * JNI bridge between the Android Java layer and the native TCMG core.
 *
 * Exported JNI methods  (Java class: com.tcmg.app.TcmgNative)
 * ──────────────────────────────────────────────────────────────
 *   jint    startServer(String cfgDir, int debugLvl)
 *   void    stopServer()
 *   jboolean isRunning()
 *   jstring getLogLines(int fromId, int maxLines, int[] outNextId)
 *   jint    getWebifPort()
 *
 * Thread safety
 * ─────────────
 * g_server_running is written from the server thread and read from the JNI
 * thread; access is guarded by g_state_mutex to avoid data races.
 * g_running / g_restart are volatile int32_t (defined in tcmg.c); writes
 * from stopServer() are visible to the server loop via the volatile qualifier.
 */

#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Logging ─────────────────────────────────────────────────────────────── */
#define LOG_TAG "TCMG-JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── External symbols from native core ──────────────────────────────────── */
/* tcmg_server_main is the renamed main() from tcmg.c (-Dmain=tcmg_server_main) */
extern int tcmg_server_main(int argc, char *argv[]);

/* Volatile flags defined in tcmg.c — written here, read in server loop */
extern volatile int32_t g_running;
extern volatile int32_t g_restart;

/* Log ring-buffer API from tcmg-log.c */
extern int32_t log_ring_since(int32_t from_id, char **out_lines,
                               int32_t max_lines, int32_t *out_next);

/* g_cfg defined in tcmg.c — we only need the webif_port field */
#include "tcmg/tcmg-globals.h"
extern S_CONFIG g_cfg;

/* ── Server state (protected by g_state_mutex) ───────────────────────────── */
static pthread_mutex_t g_state_mutex    = PTHREAD_MUTEX_INITIALIZER;
static atomic_int      g_server_running = ATOMIC_VAR_INIT(0);

/* ── Server thread argument ──────────────────────────────────────────────── */
typedef struct {
    char cfgdir[512];
    int  debug_level;
} ServerArgs;

/* ── Server thread ───────────────────────────────────────────────────────── */
static void *server_thread_fn(void *arg)
{
    ServerArgs *a = (ServerArgs *)arg;

    /* Build argv on the stack — strings are short-lived local copies */
    char  *argv[8];
    int    argc    = 0;
    char   dbg_str[16];

    argv[argc++] = "tcmg";

    if (a->debug_level > 0) {
        argv[argc++] = "-d";
        snprintf(dbg_str, sizeof(dbg_str), "%d", a->debug_level);
        argv[argc++] = dbg_str;
    }

    if (a->cfgdir[0] != '\0') {
        argv[argc++] = "-c";
        argv[argc++] = a->cfgdir;
    }

    LOGI("server thread started (cfgdir=%s debug=%d)", a->cfgdir, a->debug_level);
    free(a);

    /*
     * Reset g_running = 1 before every start.
     * tcmg.c sets g_running = 0 when it exits, so without this reset a
     * second start would return immediately.
     */
    g_running = 1;
    g_restart = 0;

    int rc = tcmg_server_main(argc, argv);

    LOGI("server thread exited (rc=%d)", rc);
    atomic_store(&g_server_running, 0);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * startServer
 * ══════════════════════════════════════════════════════════════════════════ */
JNIEXPORT jint JNICALL
Java_com_tcmg_app_TcmgNative_startServer(
        JNIEnv *env, jclass clazz,
        jstring cfgdir_j, jint debug_level)
{
    (void)clazz; /* unused — static JNI method */

    pthread_mutex_lock(&g_state_mutex);
    int already = atomic_load(&g_server_running);
    pthread_mutex_unlock(&g_state_mutex);

    if (already) {
        LOGW("startServer: already running");
        return -1;
    }

    ServerArgs *args = calloc(1, sizeof(ServerArgs));
    if (!args) {
        LOGE("startServer: calloc failed (OOM)");
        return -2;
    }

    if (cfgdir_j) {
        const char *s = (*env)->GetStringUTFChars(env, cfgdir_j, NULL);
        if (s) {
            strncpy(args->cfgdir, s, sizeof(args->cfgdir) - 1);
            /* args->cfgdir is already zero-terminated by calloc */
            (*env)->ReleaseStringUTFChars(env, cfgdir_j, s);
        }
    }
    args->debug_level = (int)debug_level;

    /* Mark running BEFORE creating thread to avoid a TOCTOU window */
    atomic_store(&g_server_running, 1);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t dummy_tid;
    int rc = pthread_create(&dummy_tid, &attr, server_thread_fn, args);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        LOGE("startServer: pthread_create failed (%d)", rc);
        atomic_store(&g_server_running, 0);
        free(args);
        return -3;
    }

    LOGI("startServer: thread created OK");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * stopServer
 * ══════════════════════════════════════════════════════════════════════════ */
JNIEXPORT void JNICALL
Java_com_tcmg_app_TcmgNative_stopServer(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    LOGI("stopServer: signalling server loop to exit");
    g_running = 0;
    g_restart = 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * isRunning
 * ══════════════════════════════════════════════════════════════════════════ */
JNIEXPORT jboolean JNICALL
Java_com_tcmg_app_TcmgNative_isRunning(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    return atomic_load(&g_server_running) ? JNI_TRUE : JNI_FALSE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * getLogLines
 *
 * Drains up to maxLines entries from the native ring buffer starting from
 * fromId, concatenates them with newlines, and returns a jstring.
 * outNextId[0] receives the id the caller should pass on the next call.
 * ══════════════════════════════════════════════════════════════════════════ */
JNIEXPORT jstring JNICALL
Java_com_tcmg_app_TcmgNative_getLogLines(
        JNIEnv *env, jclass clazz,
        jint from_id, jint max_lines, jintArray out_next_j)
{
    (void)clazz;

    if (max_lines <= 0 || max_lines > 500) max_lines = 200;

    char  **lines   = calloc((size_t)max_lines, sizeof(char *));
    int32_t next_id = (int32_t)from_id; /* fallback: return caller's id on OOM */

    if (!lines) {
        if (out_next_j) {
            jint ni = (jint)next_id;
            (*env)->SetIntArrayRegion(env, out_next_j, 0, 1, &ni);
        }
        return (*env)->NewStringUTF(env, "");
    }

    int32_t count = log_ring_since((int32_t)from_id, lines, (int32_t)max_lines, &next_id);

    /* Always update the caller's next-id, even if count == 0 */
    if (out_next_j) {
        jint ni = (jint)next_id;
        (*env)->SetIntArrayRegion(env, out_next_j, 0, 1, &ni);
    }

    if (count <= 0) {
        free(lines);
        return (*env)->NewStringUTF(env, "");
    }

    /* Calculate total byte length */
    size_t total = 0;
    for (int32_t i = 0; i < count; i++) {
        total += lines[i] ? strlen(lines[i]) : 0;
        total += 1; /* newline */
    }

    char *buf = malloc(total + 1);
    if (!buf) {
        for (int32_t i = 0; i < count; i++) free(lines[i]);
        free(lines);
        return (*env)->NewStringUTF(env, "");
    }

    size_t pos = 0;
    for (int32_t i = 0; i < count; i++) {
        if (lines[i]) {
            size_t len = strlen(lines[i]);
            memcpy(buf + pos, lines[i], len);
            pos += len;
            free(lines[i]);
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    free(lines);

    jstring result = (*env)->NewStringUTF(env, buf);
    free(buf);
    return result ? result : (*env)->NewStringUTF(env, "");
}

/* ══════════════════════════════════════════════════════════════════════════
 * getWebifPort
 * ══════════════════════════════════════════════════════════════════════════ */
JNIEXPORT jint JNICALL
Java_com_tcmg_app_TcmgNative_getWebifPort(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    if (!atomic_load(&g_server_running)) return -1;
    return (jint)g_cfg.webif_port;
}
