// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FOUNDATION_BATCHPROCESSINGCONTEXT_H_
#define SCANTAILOR_FOUNDATION_BATCHPROCESSINGCONTEXT_H_

#include <atomic>

namespace batch_processing {
namespace detail {
inline std::atomic<int> activeTasks{0};
}

/**
 * Marks the lifetime of a batch page task.  The shared count lets subordinate
 * work queues yield resources while the main processing pipeline is busy.
 */
class TaskScope {
 public:
  explicit TaskScope(const bool active) : m_active(active) {
    if (m_active) {
      detail::activeTasks.fetch_add(1, std::memory_order_relaxed);
    }
  }

  ~TaskScope() {
    if (m_active) {
      detail::activeTasks.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  TaskScope(const TaskScope&) = delete;
  TaskScope& operator=(const TaskScope&) = delete;

 private:
  bool m_active;
};

inline int activeTaskCount() {
  return detail::activeTasks.load(std::memory_order_relaxed);
}

inline bool isActive() {
  return activeTaskCount() > 0;
}
}  // namespace batch_processing

#endif  // SCANTAILOR_FOUNDATION_BATCHPROCESSINGCONTEXT_H_
