/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "artemis.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "class_status.h"
#include "common_throws.h"
#include "compilation_kind.h"
#include "deoptimization_kind.h"
#include "handle.h"
#include "handle_scope-inl.h"
#include "instrumentation.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class.h"
#include "nth_caller_visitor.h"
#include "oat_quick_method_header.h"
#include "obj_ptr-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"
#include "thread_state.h"

namespace art {
namespace artemis {

ArtMethod* GetCurrentMethodAt(Thread* self, int depth) {
  ScopedObjectAccess soa(self);
  NthCallerVisitor caller(self, depth, /*include_runtime_and_upcalls=*/ false);
  caller.WalkStack();
  CHECK_NE(caller.caller, nullptr);
  return caller.caller;
}

bool IsBeingInterpretedAt(Thread* self, int depth) {
  ScopedObjectAccess soa(self);
  NthCallerVisitor caller(self, depth, /*include_runtime_and_upcalls=*/ false);
  caller.WalkStack();
  CHECK_NE(caller.caller, nullptr);
  bool is_shadow_frame = (caller.GetCurrentShadowFrame() != nullptr);
  bool is_nterp_frame = (caller.GetCurrentQuickFrame() != nullptr) &&
      (caller.GetCurrentOatQuickMethodHeader()->IsNterpMethodHeader());
  return is_shadow_frame || is_nterp_frame;
}

bool IsMethodBeingManaged(Thread* self, ArtMethod* goal) {
  ScopedObjectAccess soa(self);
  bool found_goal = false;
  StackVisitor::WalkStack(
    [&](const StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
      if (goal == stack_visitor->GetMethod()) {
        found_goal = true;
        return false;
      }
      return true;
    },
    self,
    /*context=*/ nullptr,
    StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  return found_goal;
}

bool IsJitEnabled() {
  return Runtime::Current()->UseJitCompilation()
      && Runtime::Current()->GetInstrumentation()->GetCurrentInstrumentationLevel() !=
          instrumentation::Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter;
}

bool IsMethodJitCompiled(ArtMethod* method) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    return false;
  }
  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  return code_cache->ContainsPc(method->GetEntryPointFromQuickCompiledCode());
}

bool EnsureClassInitialized(Thread* self, ArtMethod* method) {
  ScopedObjectAccess soa(self);
  if (Runtime::Current()->GetRuntimeCallbacks()->IsMethodBeingInspected(method)) {
    std::string msg(method->PrettyMethod());
    msg += " is not safe to be JIT compiled: is being inspected";
    ThrowIllegalStateException(msg.c_str());
    return false;
  }
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> h_klass(hs.NewHandle(method->GetDeclaringClass()));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (!class_linker->EnsureInitialized(self, h_klass, true, true)) {
    self->AssertPendingException();
    return false;
  }
  if (UNLIKELY(!h_klass->IsInitialized())) {
    CHECK_EQ(h_klass->GetStatus(), ClassStatus::kInitialized);
    CHECK_EQ(h_klass->GetClinitThreadId(), self->GetTid());
    std::string msg(method->PrettyMethod());
    msg += " is not safe to be JIT compiled: the class is being initialized in this thread";
    ThrowIllegalStateException(msg.c_str());
    return false;
  }
  if (!h_klass->IsVisiblyInitialized()) {
    ScopedThreadSuspension sts(self, ThreadState::kNative);
    class_linker->MakeInitializedClassesVisiblyInitialized(self, /*wait=*/ true);
  }
  return true;
}

bool ForceJitCompileMethod(Thread* self,
                           ArtMethod* method) REQUIRES(!Locks::mutator_lock_) {
  // Do not support native methods
  if (!IsJitEnabled() || method->IsNative()) {
    return false;
  }

  // TODO(congli): See tests/common/runtime_state.cc:ForceJitCompiled()
  if (Runtime::Current()->GetInstrumentation()->EntryExitStubsInstalled() &&
      (method->IsNative() || !Runtime::Current()->IsJavaDebuggable())) {
    return false;
  }

  // The method is already JIT compiled
  if (IsMethodJitCompiled(method)) {
    return true;
  }

  // The method is being managed on the stack which requires
  // an OSR compilation. Do not support OSR in this method.
  // Use art::artemis::EnsureJitCompiled() instead.
  if (IsMethodBeingManaged(self, method)) {
    return false;
  }

  // Ensure the class is already initialized.
  // TODO(congli): Do we really need this?
  if (!EnsureClassInitialized(self, method)) {
    return false;
  }

  // Keep compiling the method until success
  jit::Jit* jit = Runtime::Current()->GetJit();
  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  code_cache->SetGarbageCollectCode(false);
  do {
    usleep(500);
    ScopedObjectAccess soa(self);
    jit->CompileMethod(method, self, CompilationKind::kOptimized, /*prejit=*/ false);
  } while (!code_cache->ContainsPc(method->GetEntryPointFromQuickCompiledCode()));

  return true;
}

bool IsArtemisEnsureJitCompiled(ArtMethod* method) {
  return method == jni::DecodeArtMethod(WellKnownClasses::dalvik_artemis_Artemis_ensureJitCompiled);
}

bool EnsureJitCompiled(Thread* self) REQUIRES(!Locks::mutator_lock_) {
  // Note, use 1 instead of 0 because the stack top is the
  // native JNI method and we are caller of that method
  ArtMethod* method = GetCurrentMethodAt(self, /*depth=*/ 1);

  // Do not support native methods
  if (!IsJitEnabled() || method->IsNative()) {
    return false;
  }

  // TODO(congli): See tests/common/runtime_state.cc:ForceJitCompiled()
  if (Runtime::Current()->GetInstrumentation()->EntryExitStubsInstalled() &&
      (method->IsNative() || !Runtime::Current()->IsJavaDebuggable())) {
    return false;
  }

  // No need to JIT compile already jitted methods
  if (IsMethodJitCompiled(method)) {
    return true;
  }

  // Keep OSR compiling the method until success
  jit::Jit* jit = Runtime::Current()->GetJit();
  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  code_cache->SetGarbageCollectCode(false);
  do {
    usleep(500);
    ScopedObjectAccess soa(self);
    // The branch will automatically invoke MaybeDoOnStackReplacement() to OSR.
    // BUG(congli): It seems MaybeDoOnStackReplacement() is never executed but
    // IsBeingInterpretedAt(self, 1) returns false. Looks like somewhere else
    // invokes PrepareForOsr()... But I did not find anywhere except nterp.
    jit->CompileMethod(method, self, CompilationKind::kOsr, /*prejit=*/ false);
    if (code_cache->LookupOsrMethodHeader(method) != nullptr) {
      break;
    }
  } while (true);

  return true;
}

bool ForceDeoptimizeMethod(Thread* self,
                           ArtMethod* method) REQUIRES(!Locks::mutator_lock_) {
  // Do not support native methods
  if (method->IsNative()) {
    return false;
  }

  // The method is not compiled, directly return.
  if (!IsMethodJitCompiled(method)) {
    return true;
  }

  // The method is being managed on the stack which requires
  // a stack transformation (quick to shadow). Do not support
  // this in this method. Use art::artemis::EnsureDeoptimized().
  // We never support deoptimize one of our caller.
  if (IsMethodBeingManaged(self, method)) {
    return false;
  }

  // Ensure the class is already initialized.
  // TODO(congli): Do we really need this?
  if (!EnsureClassInitialized(self, method)) {
    return false;
  }

  // The method is not being managed. This means we can directly invalidate the
  // jit cache and replace the method entry to the interpreter bridge
  jit::Jit* jit = Runtime::Current()->GetJit();
  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  CHECK(code_cache->ContainsMethod(method));
  CHECK(code_cache->ContainsPc(method->GetEntryPointFromQuickCompiledCode()));

  {
    ScopedObjectAccess soa(self);

    Runtime::Current()->IncrementDeoptimizationCount(DeoptimizationKind::kArtemis);
    jit->ArtemisTraceMethodDeoptimized(method);

    // Don't check OSR, otherwise, the method is just removed from osr_method_map_
    // to indicate that this method is no longer OSR compiled. But the entrypoint
    // of the method is not updated which means it is still a jitted method.
    uintptr_t pc = reinterpret_cast<uintptr_t>(method->GetEntryPointFromQuickCompiledCode());
    OatQuickMethodHeader* header = code_cache->LookupMethodHeader(pc + 1, method);
    // OatQuickMethodHeader* header = nullptr;
    // if (code_cache->IsOsrCompiled(method)) {
    //   header = code_cache->LookupOsrMethodHeader(method);
    // } else {
    //   uintptr_t pc = reinterpret_cast<uintptr_t>(method->GetEntryPointFromQuickCompiledCode());
    //   header = code_cache->LookupMethodHeader(pc + 1, method);
    // }
    CHECK_NE(header, nullptr);
    // Should notice that JitCodeCache::InvalidateCompiledCodeFor() only changes the
    // entrypoint of method but does not physically remove method from method_code_map_.
    // In such case, when method is hot again, ART no longer needs to re-compile them.
    code_cache->InvalidateCompiledCodeFor(method, header);
  }

  return true;
}

bool IsArtemisEnsureDeoptimized(ArtMethod* method) {
  return method == jni::DecodeArtMethod(WellKnownClasses::dalvik_artemis_Artemis_ensureDeoptimized);
}

extern "C" NO_RETURN void artDeoptimizeFromCompiledCode(DeoptimizationKind kind, Thread* self);

bool EnsureDeoptimized(Thread* self) REQUIRES(!Locks::mutator_lock_) {
  // Note, use 1 instead of 0 because the stack top is the
  // native JNI method and we are caller of that method
  ArtMethod* method = GetCurrentMethodAt(self, /*depth=*/ 1);

  // Do not support native methods
  if (method->IsNative()) {
    return false;
  }

  // Not JIT compiled, no need deoptimizing
  if (!IsMethodJitCompiled(method)) {
    return true;
  }

  {
    // The following code transforms quick frame to shadow frame and
    // long-jumps to artQuickToInterpreterBridge, which resumes execution
    // by knowing that it comes from a deoptimization, and thereby it
    // enters HandleDeoptimization() and EnterInterpreterFromDeoptimization().
    // EnterInterpreterFromDeoptimization() will continue the execution from
    // the transformed shadow frame.
    ScopedObjectAccess soa(self);
    artDeoptimizeFromCompiledCode(DeoptimizationKind::kArtemis, self);
    LOG(FATAL) << "UNREACHABLE";  // Expected to take long jump.
    UNREACHABLE();
  }
}

} // namespace artemis
} // namespace art
