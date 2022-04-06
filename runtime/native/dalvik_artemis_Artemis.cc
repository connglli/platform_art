/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "dalvik_artemis_Artemis.h"

#include <iostream>

#include "nativehelper/jni_macros.h"
#include "native_util.h"

namespace art {

static jboolean Artemis_helloJniArtemis(JNIEnv*, jclass) {
  std::cout << "Hello JNI Artemis" << std::endl;
  return true;
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Artemis, helloJniArtemis, "()Z"),
};

void register_dalvik_artemis_Artemis(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/artemis/Artemis");
}

}  // namespace art
