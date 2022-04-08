#include "dalvik_artemis_Artemis.h"

#include "artemis.h"
#include "art_method.h"
#include "nativehelper/jni_macros.h"
#include "native_util.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"

namespace art {

// public static native boolean isJitEnabled()

static jboolean Artemis_isJitEnabled(JNIEnv*, jclass) {
  return artemis::IsJitEnabled();
}

// public static native boolean isBeingInterpreted()

static jboolean Artemis_isBeingInterpreted(JNIEnv* env, jclass) {
  // Depth is 1 because the method to ask is our (we are the native JNI method) caller
  return artemis::IsBeingInterpretedAt(ThreadForEnv(env), /*depth=*/ 1);
}

// public static native boolean isJitCompiled()

static jboolean Artemis_isJitCompiled(JNIEnv* env, jclass) {
  // Depth is 1 because the method to ask is our (we are the native JNI method) caller
  return artemis::IsMethodJitCompiled(artemis::GetCurrentMethodAt(ThreadForEnv(env), /*depth=*/ 1));
}

// public static native boolean isMethodJitCompiled(Method method)

static jboolean Artemis_isMethodJitCompiled(JNIEnv* env, jclass, jobject java_method) {
  ArtMethod* method;
  {
    ScopedObjectAccess soa(env);
    method = ArtMethod::FromReflectedMethod(soa, java_method);
  }
  return artemis::IsMethodJitCompiled(method);
}

// public static native boolean ensureMethodJitCompiled(Method method);

static jboolean Artemis_ensureMethodJitCompiled(JNIEnv* env, jclass, jobject java_method) {
  ArtMethod* method;
  {
    ScopedObjectAccess soa(env);
    method = ArtMethod::FromReflectedMethod(soa, java_method);
  }
  return artemis::ForceJitCompileMethod(ThreadForEnv(env), method);
}

// public static native boolean ensureJitCompiled();

static jboolean Artemis_ensureJitCompiled(JNIEnv* env, jclass) {
  return artemis::EnsureJitCompiled(ThreadForEnv(env));
}

// public static native ensureMethodDeoptimized(Method method);

static jboolean Artemis_ensureMethodDeoptimized(JNIEnv* env, jclass, jobject java_method) {
  ArtMethod* method;
  {
    ScopedObjectAccess soa(env);
    method = ArtMethod::FromReflectedMethod(soa, java_method);
  }
  return artemis::ForceDeoptimizeMethod(ThreadForEnv(env), method);
}

// public static native boolean ensureDeoptimized();

static void Artemis_ensureDeoptimized(JNIEnv* env, jclass) {
  artemis::EnsureDeoptimized(ThreadForEnv(env));
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Artemis, isJitEnabled, "()Z"),
  NATIVE_METHOD(Artemis, isBeingInterpreted, "()Z"),
  NATIVE_METHOD(Artemis, isJitCompiled, "()Z"),
  NATIVE_METHOD(Artemis, isMethodJitCompiled, "(Ljava/lang/reflect/Method;)Z"),
  NATIVE_METHOD(Artemis, ensureJitCompiled, "()Z"),
  NATIVE_METHOD(Artemis, ensureMethodJitCompiled, "(Ljava/lang/reflect/Method;)Z"),
  NATIVE_METHOD(Artemis, ensureDeoptimized, "()V"),
  NATIVE_METHOD(Artemis, ensureMethodDeoptimized, "(Ljava/lang/reflect/Method;)Z"),
};

void register_dalvik_artemis_Artemis(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/artemis/Artemis");
}

}  // namespace art
