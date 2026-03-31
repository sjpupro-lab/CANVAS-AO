#ifdef __ANDROID__
#include <jni.h>
#include "canvasos.h"
#include "cell.h"

/* Forward declarations */
void          dating_init(void);
const char   *dating_respond(const char *input);
EmotionVector *dating_get_emotion(void);

JNIEXPORT void JNICALL
Java_com_sjcanvas_dating_DatingEngine_init(JNIEnv *env, jobject obj) {
    (void)env; (void)obj;
    dating_init();
}

JNIEXPORT jstring JNICALL
Java_com_sjcanvas_dating_DatingEngine_respond(JNIEnv *env, jobject obj,
                                               jstring input) {
    (void)obj;
    const char *inp = (*env)->GetStringUTFChars(env, input, NULL);
    const char *resp = dating_respond(inp);
    (*env)->ReleaseStringUTFChars(env, input, inp);
    return (*env)->NewStringUTF(env, resp);
}

JNIEXPORT jfloatArray JNICALL
Java_com_sjcanvas_dating_DatingEngine_getEmotion(JNIEnv *env, jobject obj) {
    (void)obj;
    EmotionVector *ev = dating_get_emotion();
    jfloatArray arr   = (*env)->NewFloatArray(env, 7);
    jfloat vals[7] = {
        ev->joy, ev->trust, ev->fear, ev->surprise,
        ev->sadness, ev->disgust, ev->anger
    };
    (*env)->SetFloatArrayRegion(env, arr, 0, 7, vals);
    return arr;
}

#endif /* __ANDROID__ */
