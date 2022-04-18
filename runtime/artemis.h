#ifndef ART_RUNTIME_ARTEMIS_H_
#define ART_RUNTIME_ARTEMIS_H_

#include "art_method.h"
#include "thread.h"

namespace art {
namespace artemis {

/**
 * Get the current method at self stack's depth
 */
ArtMethod* GetCurrentMethodAt(Thread* self, int depth);

/**
 * Return whether the method at self's stack depth is being interpreted.
 */
bool IsBeingInterpretedAt(Thread* self, int depth);

/**
 * Return whether JIT compilation is enabled
 */
bool IsJitEnabled();

/**
 * Return whether a method is already JIT compiled
 */
bool IsMethodJitCompiled(ArtMethod* method);

/**
 * Force Jit to compile the given method. Return whehter
 * the method is successfully JIT compiled.
 */
bool ForceJitCompileMethod(Thread* self,
                           ArtMethod* method) REQUIRES(!Locks::mutator_lock_);

/**
 * Return whether the given method is dalvik_artemis_Artemis_ensureJitCompiled.
 */
bool IsArtemisEnsureJitCompiled(ArtMethod* method);

/**
 * Force Jit to compile current method. Return whehter
 * the current method is successfully JIT compiled.
 */
bool EnsureJitCompiled(Thread* self) REQUIRES(!Locks::mutator_lock_);

/**
 * Force to deoptimize the given method. Return whehter
 * the method is successfully deoptimized.
 */
bool ForceDeoptimizeMethod(Thread* self,
                           ArtMethod* method) REQUIRES(!Locks::mutator_lock_);

/**
 * Return whether the given method is dalvik_artemis_Artemis_ensureDeoptimized.
 */
bool IsArtemisEnsureDeoptimized(ArtMethod* method);

/**
 * Force to deoptimize current method. Return whehter
 * the current method is successfully deoptimized.
 */
bool EnsureDeoptimized(Thread* self) REQUIRES(!Locks::mutator_lock_);

} // namespace artemis
} // namespace art

#endif // ART_RUNTIME_ARTEMIS_H_
