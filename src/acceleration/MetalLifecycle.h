// Copyright (C) 2024 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef METAL_LIFECYCLE_H_
#define METAL_LIFECYCLE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the lifecycle observer. Call once at app startup.
 * Safe to call multiple times - subsequent calls are no-ops.
 */
void metalLifecycleInit(void);

/**
 * Check if the app is currently active (in foreground).
 * Returns false if app is backgrounded or inactive.
 * GPU operations should skip when this returns false to avoid crashes
 * from purged GPU resources.
 */
bool metalIsAppActive(void);

#ifdef __cplusplus
}
#endif

#endif  // METAL_LIFECYCLE_H_
