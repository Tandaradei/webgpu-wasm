/**
 * @license
 * Copyright 2011 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

// === Auto-generated postamble setup entry stuff ===

{{GLOBAL_VARS}}

if (runtimeInitialized) {
  // dlopen case: we are being loaded after the system is fully initialized, so just run our prerun and atinit stuff now
  callRuntimeCallbacks(__ATPRERUN__);
  callRuntimeCallbacks(__ATINIT__);
} // otherwise, general dynamic linking case: stuff we added to prerun and init will be executed with the rest of the system as it loads

