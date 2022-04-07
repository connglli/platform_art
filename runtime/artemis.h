#ifndef ART_RUNTIME_ARTEMIS_H_
#define ART_RUNTIME_ARTEMIS_H_

#include "thread.h"
#include "art_method.h"

namespace art {
namespace artemis {

/**
 * Get the current method on self's stack top
 */
ArtMethod* GetCurrentMethod(Thread* self);

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
 * Force to deoptimize the given method. Return whehter
 * the method is successfully deoptimized.
 */
bool ForceDeoptimizeMethod(Thread* self,
                           ArtMethod* method) REQUIRES(!Locks::mutator_lock_);

} // namespace artemis
} // namespace art

#endif // ART_RUNTIME_ARTEMIS_H_
