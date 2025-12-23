// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BackgroundExecutor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <atomic>

#include "OutOfMemoryHandler.h"

class BackgroundExecutor::Dispatcher : public QObject {
 public:
  explicit Dispatcher(Impl& owner);

 protected:
  void customEvent(QEvent* event) override;

 private:
  Impl& m_owner;
};


class BackgroundExecutor::Impl : public QThread {
 public:
  explicit Impl(BackgroundExecutor& owner);

  ~Impl() override;

  void enqueueTask(const TaskPtr& task);

 protected:
  void run() override;

  void customEvent(QEvent* event) override;

 private:
  BackgroundExecutor& m_owner;
  Dispatcher m_dispatcher;
  std::atomic<bool> m_threadStarted{false};
};


/*============================ BackgroundExecutor ==========================*/

BackgroundExecutor::BackgroundExecutor() : m_impl(std::make_unique<Impl>(*this)) {}

BackgroundExecutor::~BackgroundExecutor() = default;

void BackgroundExecutor::shutdown() {
  m_impl.reset();
}

void BackgroundExecutor::enqueueTask(const TaskPtr& task) {
  if (m_impl) {
    m_impl->enqueueTask(task);
  }
}

/*===================== BackgroundExecutor::Dispatcher =====================*/

BackgroundExecutor::Dispatcher::Dispatcher(Impl& owner) : m_owner(owner) {}

void BackgroundExecutor::Dispatcher::customEvent(QEvent* event) {
  try {
    auto* evt = dynamic_cast<TaskEvent*>(event);
    if (!evt) {
      qWarning() << "BackgroundExecutor::Dispatcher: received invalid event type";
      return;
    }

    const TaskPtr& task = evt->payload();
    if (!task) {
      qWarning() << "BackgroundExecutor::Dispatcher: received null task";
      return;
    }

    const TaskResultPtr result((*task)());
    if (result) {
      QCoreApplication::postEvent(&m_owner, new ResultEvent(result));
    }
  } catch (const std::bad_alloc&) {
    OutOfMemoryHandler::instance().handleOutOfMemorySituation();
  }
}

/*======================= BackgroundExecutor::Impl =========================*/

BackgroundExecutor::Impl::Impl(BackgroundExecutor& owner)
    : m_owner(owner), m_dispatcher(*this) {
  m_dispatcher.moveToThread(this);
}

BackgroundExecutor::Impl::~Impl() {
  exit();
  wait();
}

void BackgroundExecutor::Impl::enqueueTask(const TaskPtr& task) {
  QCoreApplication::postEvent(&m_dispatcher, new TaskEvent(task));
  // Use atomic compare-exchange to ensure thread is started exactly once
  bool expected = false;
  if (m_threadStarted.compare_exchange_strong(expected, true)) {
    start();
  }
}

void BackgroundExecutor::Impl::run() {
  exec();
}

void BackgroundExecutor::Impl::customEvent(QEvent* event) {
  auto* evt = dynamic_cast<ResultEvent*>(event);
  if (!evt) {
    qWarning() << "BackgroundExecutor::Impl: received invalid event type";
    return;
  }

  const TaskResultPtr& result = evt->payload();
  if (!result) {
    qWarning() << "BackgroundExecutor::Impl: received null result";
    return;
  }

  (*result)();
}
