// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy_impl.h"
#include "base/pending_task.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#include "base/win/scoped_handle.h"
#endif

namespace base {

// TODO(darin): Platform-specific MessageLoop tests should be grouped together
// to avoid chopping this file up with so many #ifdefs.

namespace {

class Foo : public RefCounted<Foo> {
 public:
  Foo() : test_count_(0) {
  }

  void Test0() {
    ++test_count_;
  }

  void Test1ConstRef(const std::string& a) {
    ++test_count_;
    result_.append(a);
  }

  void Test1Ptr(std::string* a) {
    ++test_count_;
    result_.append(*a);
  }

  void Test1Int(int a) {
    test_count_ += a;
  }

  void Test2Ptr(std::string* a, std::string* b) {
    ++test_count_;
    result_.append(*a);
    result_.append(*b);
  }

  void Test2Mixed(const std::string& a, std::string* b) {
    ++test_count_;
    result_.append(a);
    result_.append(*b);
  }

  int test_count() const { return test_count_; }
  const std::string& result() const { return result_; }

 private:
  friend class RefCounted<Foo>;

  ~Foo() {}

  int test_count_;
  std::string result_;
};

void RunTest_PostTask(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Add tests to message loop
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a"), b("b"), c("c"), d("d");
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test0, foo.get()));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
    &Foo::Test1ConstRef, foo.get(), a));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test1Ptr, foo.get(), &b));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test1Int, foo.get(), 100));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test2Ptr, foo.get(), &a, &c));

  // TryPost with no contention. It must succeed.
  EXPECT_TRUE(MessageLoop::current()->TryPostTask(FROM_HERE, Bind(
      &Foo::Test2Mixed, foo.get(), a, &d)));

  // TryPost with simulated contention. It must fail. We wait for a helper
  // thread to lock the queue, we TryPost on this thread and finally we
  // signal the helper to unlock and exit.
  WaitableEvent wait(true, false);
  WaitableEvent signal(true, false);
  Thread thread("RunTest_PostTask_helper");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE,
      Bind(&MessageLoop::LockWaitUnLockForTesting,
           base::Unretained(MessageLoop::current()),
           &wait,
           &signal));

  wait.Wait();
  EXPECT_FALSE(MessageLoop::current()->TryPostTask(FROM_HERE, Bind(
      &Foo::Test2Mixed, foo.get(), a, &d)));
  signal.Signal();

  // After all tests, post a message that will shut down the message loop
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &MessageLoop::Quit, Unretained(MessageLoop::current())));

  // Now kick things off
  MessageLoop::current()->Run();

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

void RunTest_PostTask_SEH(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Add tests to message loop
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a"), b("b"), c("c"), d("d");
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test0, foo.get()));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test1ConstRef, foo.get(), a));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test1Ptr, foo.get(), &b));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test1Int, foo.get(), 100));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test2Ptr, foo.get(), &a, &c));
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &Foo::Test2Mixed, foo.get(), a, &d));

  // After all tests, post a message that will shut down the message loop
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &MessageLoop::Quit, Unretained(MessageLoop::current())));

  // Now kick things off with the SEH block active.
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

// This function runs slowly to simulate a large amount of work being done.
static void SlowFunc(TimeDelta pause, int* quit_counter) {
    PlatformThread::Sleep(pause);
    if (--(*quit_counter) == 0)
      MessageLoop::current()->QuitWhenIdle();
}

// This function records the time when Run was called in a Time object, which is
// useful for building a variety of MessageLoop tests.
static void RecordRunTimeFunc(Time* run_time, int* quit_counter) {
  *run_time = Time::Now();

    // Cause our Run function to take some time to execute.  As a result we can
    // count on subsequent RecordRunTimeFunc()s running at a future time,
    // without worry about the resolution of our system clock being an issue.
  SlowFunc(TimeDelta::FromMilliseconds(10), quit_counter);
}

void RunTest_PostDelayedTask_Basic(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that PostDelayedTask results in a delayed task.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 1;
  Time run_time;

  loop.PostDelayedTask(
      FROM_HERE, Bind(&RecordRunTimeFunc, &run_time, &num_tasks),
      kDelay);

  Time time_before_run = Time::Now();
  loop.Run();
  Time time_after_run = Time::Now();

  EXPECT_EQ(0, num_tasks);
  EXPECT_LT(kDelay, time_after_run - time_before_run);
}

void RunTest_PostDelayedTask_InDelayOrder(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that two tasks with different delays run in the right order.
  int num_tasks = 2;
  Time run_time1, run_time2;

  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromMilliseconds(200));
  // If we get a large pause in execution (due to a context switch) here, this
  // test could fail.
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 < run_time1);
}

void RunTest_PostDelayedTask_InPostOrder(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that two tasks with the same delay run in the order in which they
  // were posted.
  //
  // NOTE: This is actually an approximate test since the API only takes a
  // "delay" parameter, so we are not exactly simulating two tasks that get
  // posted at the exact same time.  It would be nice if the API allowed us to
  // specify the desired run time.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 2;
  Time run_time1, run_time2;

  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time1, &num_tasks), kDelay);
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time2, &num_tasks), kDelay);

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time1 < run_time2);
}

void RunTest_PostDelayedTask_InPostOrder_2(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that a delayed task still runs after a normal tasks even if the
  // normal tasks take a long time to run.

  const TimeDelta kPause = TimeDelta::FromMilliseconds(50);

  int num_tasks = 2;
  Time run_time;

  loop.PostTask(FROM_HERE, Bind(&SlowFunc, kPause, &num_tasks));
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  Time time_before_run = Time::Now();
  loop.Run();
  Time time_after_run = Time::Now();

  EXPECT_EQ(0, num_tasks);

  EXPECT_LT(kPause, time_after_run - time_before_run);
}

void RunTest_PostDelayedTask_InPostOrder_3(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that a delayed task still runs after a pile of normal tasks.  The key
  // difference between this test and the previous one is that here we return
  // the MessageLoop a lot so we give the MessageLoop plenty of opportunities
  // to maybe run the delayed task.  It should know not to do so until the
  // delayed task's delay has passed.

  int num_tasks = 11;
  Time run_time1, run_time2;

  // Clutter the ML with tasks.
  for (int i = 1; i < num_tasks; ++i)
    loop.PostTask(FROM_HERE,
                  Bind(&RecordRunTimeFunc, &run_time1, &num_tasks));

  loop.PostDelayedTask(
      FROM_HERE, Bind(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(1));

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 > run_time1);
}

void RunTest_PostDelayedTask_SharedTimer(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  Time run_time1, run_time2;

  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromSeconds(1000));
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  Time start_time = Time::Now();

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = Time::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time1.is_null());
  EXPECT_FALSE(run_time2.is_null());
}

#if defined(OS_WIN)

void SubPumpFunc() {
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  MessageLoop::current()->QuitWhenIdle();
}

void RunTest_PostDelayedTask_SharedTimer_SubPump() {
  MessageLoop loop(MessageLoop::TYPE_UI);

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  Time run_time;

  loop.PostTask(FROM_HERE, Bind(&SubPumpFunc));

  // This very delayed task should never run.
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromSeconds(1000));

  // This slightly delayed task should run from within SubPumpFunc).
  loop.PostDelayedTask(
      FROM_HERE,
      Bind(&PostQuitMessage, 0),
      TimeDelta::FromMilliseconds(10));

  Time start_time = Time::Now();

  loop.Run();
  EXPECT_EQ(1, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = Time::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time.is_null());
}

#endif  // defined(OS_WIN)

// This is used to inject a test point for recording the destructor calls for
// Closure objects send to MessageLoop::PostTask(). It is awkward usage since we
// are trying to hook the actual destruction, which is not a common operation.
class RecordDeletionProbe : public RefCounted<RecordDeletionProbe> {
 public:
  RecordDeletionProbe(RecordDeletionProbe* post_on_delete, bool* was_deleted)
      : post_on_delete_(post_on_delete), was_deleted_(was_deleted) {
  }
  void Run() {}

 private:
  friend class RefCounted<RecordDeletionProbe>;

  ~RecordDeletionProbe() {
    *was_deleted_ = true;
    if (post_on_delete_.get())
      MessageLoop::current()->PostTask(
          FROM_HERE, Bind(&RecordDeletionProbe::Run, post_on_delete_.get()));
  }

  scoped_refptr<RecordDeletionProbe> post_on_delete_;
  bool* was_deleted_;
};

void RunTest_EnsureDeletion(MessageLoop::Type message_loop_type) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  {
    MessageLoop loop(message_loop_type);
    loop.PostTask(
        FROM_HERE, Bind(&RecordDeletionProbe::Run,
                              new RecordDeletionProbe(NULL, &a_was_deleted)));
    // TODO(ajwong): Do we really need 1000ms here?
    loop.PostDelayedTask(
        FROM_HERE, Bind(&RecordDeletionProbe::Run,
                              new RecordDeletionProbe(NULL, &b_was_deleted)),
        TimeDelta::FromMilliseconds(1000));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
}

void RunTest_EnsureDeletion_Chain(MessageLoop::Type message_loop_type) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  bool c_was_deleted = false;
  {
    MessageLoop loop(message_loop_type);
    // The scoped_refptr for each of the below is held either by the chained
    // RecordDeletionProbe, or the bound RecordDeletionProbe::Run() callback.
    RecordDeletionProbe* a = new RecordDeletionProbe(NULL, &a_was_deleted);
    RecordDeletionProbe* b = new RecordDeletionProbe(a, &b_was_deleted);
    RecordDeletionProbe* c = new RecordDeletionProbe(b, &c_was_deleted);
    loop.PostTask(FROM_HERE, Bind(&RecordDeletionProbe::Run, c));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
  EXPECT_TRUE(c_was_deleted);
}

void NestingFunc(int* depth) {
  if (*depth > 0) {
    *depth -= 1;
    MessageLoop::current()->PostTask(FROM_HERE,
                                     Bind(&NestingFunc, depth));

    MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->Run();
  }
  MessageLoop::current()->QuitWhenIdle();
}

#if defined(OS_WIN)

LONG WINAPI BadExceptionHandler(EXCEPTION_POINTERS *ex_info) {
  ADD_FAILURE() << "bad exception handler";
  ::ExitProcess(ex_info->ExceptionRecord->ExceptionCode);
  return EXCEPTION_EXECUTE_HANDLER;
}

// This task throws an SEH exception: initially write to an invalid address.
// If the right SEH filter is installed, it will fix the error.
class Crasher : public RefCounted<Crasher> {
 public:
  // Ctor. If trash_SEH_handler is true, the task will override the unhandled
  // exception handler with one sure to crash this test.
  explicit Crasher(bool trash_SEH_handler)
      : trash_SEH_handler_(trash_SEH_handler) {
  }

  void Run() {
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
    if (trash_SEH_handler_)
      ::SetUnhandledExceptionFilter(&BadExceptionHandler);
    // Generate a SEH fault. We do it in asm to make sure we know how to undo
    // the damage.

#if defined(_M_IX86)

    __asm {
      mov eax, dword ptr [Crasher::bad_array_]
      mov byte ptr [eax], 66
    }

#elif defined(_M_X64)

    bad_array_[0] = 66;

#else
#error "needs architecture support"
#endif

    MessageLoop::current()->QuitWhenIdle();
  }
  // Points the bad array to a valid memory location.
  static void FixError() {
    bad_array_ = &valid_store_;
  }

 private:
  bool trash_SEH_handler_;
  static volatile char* bad_array_;
  static char valid_store_;
};

volatile char* Crasher::bad_array_ = 0;
char Crasher::valid_store_ = 0;

// This SEH filter fixes the problem and retries execution. Fixing requires
// that the last instruction: mov eax, [Crasher::bad_array_] to be retried
// so we move the instruction pointer 5 bytes back.
LONG WINAPI HandleCrasherException(EXCEPTION_POINTERS *ex_info) {
  if (ex_info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
    return EXCEPTION_EXECUTE_HANDLER;

  Crasher::FixError();

#if defined(_M_IX86)

  ex_info->ContextRecord->Eip -= 5;

#elif defined(_M_X64)

  ex_info->ContextRecord->Rip -= 5;

#endif

  return EXCEPTION_CONTINUE_EXECUTION;
}

void RunTest_Crasher(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  if (::IsDebuggerPresent())
    return;

  LPTOP_LEVEL_EXCEPTION_FILTER old_SEH_filter =
      ::SetUnhandledExceptionFilter(&HandleCrasherException);

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&Crasher::Run, new Crasher(false)));
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  ::SetUnhandledExceptionFilter(old_SEH_filter);
}

void RunTest_CrasherNasty(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  if (::IsDebuggerPresent())
    return;

  LPTOP_LEVEL_EXCEPTION_FILTER old_SEH_filter =
      ::SetUnhandledExceptionFilter(&HandleCrasherException);

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&Crasher::Run, new Crasher(true)));
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  ::SetUnhandledExceptionFilter(old_SEH_filter);
}

#endif  // defined(OS_WIN)

void RunTest_Nesting(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  int depth = 100;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   Bind(&NestingFunc, &depth));
  MessageLoop::current()->Run();
  EXPECT_EQ(depth, 0);
}

const wchar_t* const kMessageBoxTitle = L"MessageLoop Unit Test";

enum TaskType {
  MESSAGEBOX,
  ENDDIALOG,
  RECURSIVE,
  TIMEDMESSAGELOOP,
  QUITMESSAGELOOP,
  ORDERED,
  PUMPS,
  SLEEP,
  RUNS,
};

// Saves the order in which the tasks executed.
struct TaskItem {
  TaskItem(TaskType t, int c, bool s)
      : type(t),
        cookie(c),
        start(s) {
  }

  TaskType type;
  int cookie;
  bool start;

  bool operator == (const TaskItem& other) const {
    return type == other.type && cookie == other.cookie && start == other.start;
  }
};

std::ostream& operator <<(std::ostream& os, TaskType type) {
  switch (type) {
  case MESSAGEBOX:        os << "MESSAGEBOX"; break;
  case ENDDIALOG:         os << "ENDDIALOG"; break;
  case RECURSIVE:         os << "RECURSIVE"; break;
  case TIMEDMESSAGELOOP:  os << "TIMEDMESSAGELOOP"; break;
  case QUITMESSAGELOOP:   os << "QUITMESSAGELOOP"; break;
  case ORDERED:          os << "ORDERED"; break;
  case PUMPS:             os << "PUMPS"; break;
  case SLEEP:             os << "SLEEP"; break;
  default:
    NOTREACHED();
    os << "Unknown TaskType";
    break;
  }
  return os;
}

std::ostream& operator <<(std::ostream& os, const TaskItem& item) {
  if (item.start)
    return os << item.type << " " << item.cookie << " starts";
  else
    return os << item.type << " " << item.cookie << " ends";
}

class TaskList {
 public:
  void RecordStart(TaskType type, int cookie) {
    TaskItem item(type, cookie, true);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  void RecordEnd(TaskType type, int cookie) {
    TaskItem item(type, cookie, false);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  size_t Size() {
    return task_list_.size();
  }

  TaskItem Get(int n)  {
    return task_list_[n];
  }

 private:
  std::vector<TaskItem> task_list_;
};

// Saves the order the tasks ran.
void OrderedFunc(TaskList* order, int cookie) {
  order->RecordStart(ORDERED, cookie);
  order->RecordEnd(ORDERED, cookie);
}

#if defined(OS_WIN)

// MessageLoop implicitly start a "modal message loop". Modal dialog boxes,
// common controls (like OpenFile) and StartDoc printing function can cause
// implicit message loops.
void MessageBoxFunc(TaskList* order, int cookie, bool is_reentrant) {
  order->RecordStart(MESSAGEBOX, cookie);
  if (is_reentrant)
    MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageBox(NULL, L"Please wait...", kMessageBoxTitle, MB_OK);
  order->RecordEnd(MESSAGEBOX, cookie);
}

// Will end the MessageBox.
void EndDialogFunc(TaskList* order, int cookie) {
  order->RecordStart(ENDDIALOG, cookie);
  HWND window = GetActiveWindow();
  if (window != NULL) {
    EXPECT_NE(EndDialog(window, IDCONTINUE), 0);
    // Cheap way to signal that the window wasn't found if RunEnd() isn't
    // called.
    order->RecordEnd(ENDDIALOG, cookie);
  }
}

#endif  // defined(OS_WIN)

void RecursiveFunc(TaskList* order, int cookie, int depth,
                   bool is_reentrant) {
  order->RecordStart(RECURSIVE, cookie);
  if (depth > 0) {
    if (is_reentrant)
      MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        Bind(&RecursiveFunc, order, cookie, depth - 1, is_reentrant));
  }
  order->RecordEnd(RECURSIVE, cookie);
}

void RecursiveSlowFunc(TaskList* order, int cookie, int depth,
                       bool is_reentrant) {
  RecursiveFunc(order, cookie, depth, is_reentrant);
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
}

void QuitFunc(TaskList* order, int cookie) {
  order->RecordStart(QUITMESSAGELOOP, cookie);
  MessageLoop::current()->QuitWhenIdle();
  order->RecordEnd(QUITMESSAGELOOP, cookie);
}

void SleepFunc(TaskList* order, int cookie, TimeDelta delay) {
  order->RecordStart(SLEEP, cookie);
  PlatformThread::Sleep(delay);
  order->RecordEnd(SLEEP, cookie);
}

#if defined(OS_WIN)
void RecursiveFuncWin(MessageLoop* target,
                      HANDLE event,
                      bool expect_window,
                      TaskList* order,
                      bool is_reentrant) {
  target->PostTask(FROM_HERE,
                   Bind(&RecursiveFunc, order, 1, 2, is_reentrant));
  target->PostTask(FROM_HERE,
                   Bind(&MessageBoxFunc, order, 2, is_reentrant));
  target->PostTask(FROM_HERE,
                   Bind(&RecursiveFunc, order, 3, 2, is_reentrant));
  // The trick here is that for recursive task processing, this task will be
  // ran _inside_ the MessageBox message loop, dismissing the MessageBox
  // without a chance.
  // For non-recursive task processing, this will be executed _after_ the
  // MessageBox will have been dismissed by the code below, where
  // expect_window_ is true.
  target->PostTask(FROM_HERE,
                   Bind(&EndDialogFunc, order, 4));
  target->PostTask(FROM_HERE,
                   Bind(&QuitFunc, order, 5));

  // Enforce that every tasks are sent before starting to run the main thread
  // message loop.
  ASSERT_TRUE(SetEvent(event));

  // Poll for the MessageBox. Don't do this at home! At the speed we do it,
  // you will never realize one MessageBox was shown.
  for (; expect_window;) {
    HWND window = FindWindow(L"#32770", kMessageBoxTitle);
    if (window) {
      // Dismiss it.
      for (;;) {
        HWND button = FindWindowEx(window, NULL, L"Button", NULL);
        if (button != NULL) {
          EXPECT_EQ(0, SendMessage(button, WM_LBUTTONDOWN, 0, 0));
          EXPECT_EQ(0, SendMessage(button, WM_LBUTTONUP, 0, 0));
          break;
        }
      }
      break;
    }
  }
}

#endif  // defined(OS_WIN)

void RunTest_RecursiveDenial1(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&RecursiveFunc, &order, 1, 2, false));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&RecursiveFunc, &order, 2, 2, false));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&QuitFunc, &order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

void RunTest_RecursiveDenial3(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&RecursiveSlowFunc, &order, 1, 2, false));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&RecursiveSlowFunc, &order, 2, 2, false));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      Bind(&OrderedFunc, &order, 3),
      TimeDelta::FromMilliseconds(5));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      Bind(&QuitFunc, &order, 4),
      TimeDelta::FromMilliseconds(5));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(16U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(5), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(7), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 4, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 4, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 2, false));
}

void RunTest_RecursiveSupport1(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&RecursiveFunc, &order, 1, 2, true));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&RecursiveFunc, &order, 2, 2, true));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&QuitFunc, &order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

#if defined(OS_WIN)
// TODO(darin): These tests need to be ported since they test critical
// message loop functionality.

// A side effect of this test is the generation a beep. Sorry.
void RunTest_RecursiveDenial2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveDenial2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.message_loop()->PostTask(FROM_HERE,
                                  Bind(&RecursiveFuncWin,
                                             MessageLoop::current(),
                                             event.Get(),
                                             true,
                                             &order,
                                             false));
  // Let the other thread execute.
  WaitForSingleObject(event, INFINITE);
  MessageLoop::current()->Run();

  ASSERT_EQ(order.Size(), 17);
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(MESSAGEBOX, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(RECURSIVE, 3, false));
  // When EndDialogFunc is processed, the window is already dismissed, hence no
  // "end" entry.
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(7), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, false));
}

// A side effect of this test is the generation a beep. Sorry.  This test also
// needs to process windows messages on the current thread.
void RunTest_RecursiveSupport2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveSupport2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.message_loop()->PostTask(FROM_HERE,
                                  Bind(&RecursiveFuncWin,
                                             MessageLoop::current(),
                                             event.Get(),
                                             false,
                                             &order,
                                             true));
  // Let the other thread execute.
  WaitForSingleObject(event, INFINITE);
  MessageLoop::current()->Run();

  ASSERT_EQ(order.Size(), 18);
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  // Note that this executes in the MessageBox modal loop.
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(5), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, false));
  EXPECT_EQ(order.Get(7), TaskItem(MESSAGEBOX, 2, false));
  /* The order can subtly change here. The reason is that when RecursiveFunc(1)
     is called in the main thread, if it is faster than getting to the
     PostTask(FROM_HERE, Bind(&QuitFunc) execution, the order of task
     execution can change. We don't care anyway that the order isn't correct.
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(9), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  */
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(17), TaskItem(RECURSIVE, 3, false));
}

#endif  // defined(OS_WIN)

void FuncThatPumps(TaskList* order, int cookie) {
  order->RecordStart(PUMPS, cookie);
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    RunLoop().RunUntilIdle();
  }
  order->RecordEnd(PUMPS, cookie);
}

void FuncThatRuns(TaskList* order, int cookie, RunLoop* run_loop) {
  order->RecordStart(RUNS, cookie);
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    run_loop->Run();
  }
  order->RecordEnd(RUNS, cookie);
}

void FuncThatQuitsNow() {
  MessageLoop::current()->QuitNow();
}

// Tests that non nestable tasks run in FIFO if there are no nested loops.
void RunTest_NonNestableWithNoNesting(
    MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  MessageLoop::current()->PostNonNestableTask(
      FROM_HERE,
      Bind(&OrderedFunc, &order, 1));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   Bind(&QuitFunc, &order, 3));
  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(6U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
}

// Tests that non nestable tasks don't run when there's code in the call stack.
void RunTest_NonNestableInNestedLoop(MessageLoop::Type message_loop_type,
                                     bool use_delayed) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&FuncThatPumps, &order, 1));
  if (use_delayed) {
    MessageLoop::current()->PostNonNestableDelayedTask(
        FROM_HERE,
        Bind(&OrderedFunc, &order, 2),
        TimeDelta::FromMilliseconds(1));
  } else {
    MessageLoop::current()->PostNonNestableTask(
        FROM_HERE,
        Bind(&OrderedFunc, &order, 2));
  }
  MessageLoop::current()->PostTask(FROM_HERE,
                                   Bind(&OrderedFunc, &order, 3));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&SleepFunc, &order, 4, TimeDelta::FromMilliseconds(50)));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   Bind(&OrderedFunc, &order, 5));
  if (use_delayed) {
    MessageLoop::current()->PostNonNestableDelayedTask(
        FROM_HERE,
        Bind(&QuitFunc, &order, 6),
        TimeDelta::FromMilliseconds(2));
  } else {
    MessageLoop::current()->PostNonNestableTask(
        FROM_HERE,
        Bind(&QuitFunc, &order, 6));
  }

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(12U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(PUMPS, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(3), TaskItem(SLEEP, 4, true));
  EXPECT_EQ(order.Get(4), TaskItem(SLEEP, 4, false));
  EXPECT_EQ(order.Get(5), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(7), TaskItem(PUMPS, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 6, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 6, false));
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
void RunTest_QuitNow(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop run_loop;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&run_loop)));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 3));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 4)); // never runs

  MessageLoop::current()->Run();

  ASSERT_EQ(6U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works before RunWithID.
void RunTest_RunLoopQuitOrderBefore(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop run_loop;

  run_loop.Quit();

  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 1)); // never runs
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow)); // never runs

  run_loop.Run();

  ASSERT_EQ(0U, order.Size());
}

// Tests RunLoopQuit works during RunWithID.
void RunTest_RunLoopQuitOrderDuring(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop run_loop;

  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 1));
  MessageLoop::current()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2)); // never runs
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow)); // never runs

  run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works after RunWithID.
void RunTest_RunLoopQuitOrderAfter(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop run_loop;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&run_loop)));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 3));
  MessageLoop::current()->PostTask(
      FROM_HERE, run_loop.QuitClosure()); // has no affect
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 4));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&FuncThatQuitsNow));

  RunLoop outer_run_loop;
  outer_run_loop.Run();

  ASSERT_EQ(8U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
void RunTest_RunLoopQuitTop(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  MessageLoop::current()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
void RunTest_RunLoopQuitNested(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
void RunTest_RunLoopQuitBogus(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;
  RunLoop bogus_run_loop;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  MessageLoop::current()->PostTask(
      FROM_HERE, bogus_run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 2));
  MessageLoop::current()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
void RunTest_RunLoopQuitDeep(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_loop1;
  RunLoop nested_loop2;
  RunLoop nested_loop3;
  RunLoop nested_loop4;

  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 1, Unretained(&nested_loop1)));
  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 2, Unretained(&nested_loop2)));
  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 3, Unretained(&nested_loop3)));
  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&FuncThatRuns, &order, 4, Unretained(&nested_loop4)));
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 5));
  MessageLoop::current()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 6));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_loop1.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 7));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_loop2.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 8));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_loop3.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 9));
  MessageLoop::current()->PostTask(
      FROM_HERE, nested_loop4.QuitClosure());
  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&OrderedFunc, &order, 10));

  outer_run_loop.Run();

  ASSERT_EQ(18U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

void PostNTasksThenQuit(int posts_remaining) {
  if (posts_remaining > 1) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        Bind(&PostNTasksThenQuit, posts_remaining - 1));
  } else {
    MessageLoop::current()->QuitWhenIdle();
  }
}

void RunTest_RecursivePosts(MessageLoop::Type message_loop_type,
                            int num_times) {
  MessageLoop loop(message_loop_type);
  loop.PostTask(FROM_HERE, Bind(&PostNTasksThenQuit, num_times));
  loop.Run();
}

#if defined(OS_WIN)

class DispatcherImpl : public MessageLoopForUI::Dispatcher {
 public:
  DispatcherImpl() : dispatch_count_(0) {}

  virtual bool Dispatch(const NativeEvent& msg) OVERRIDE {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    // Do not count WM_TIMER since it is not what we post and it will cause
    // flakiness.
    if (msg.message != WM_TIMER)
      ++dispatch_count_;
    // We treat WM_LBUTTONUP as the last message.
    return msg.message != WM_LBUTTONUP;
  }

  int dispatch_count_;
};

void MouseDownUp() {
  PostMessage(NULL, WM_LBUTTONDOWN, 0, 0);
  PostMessage(NULL, WM_LBUTTONUP, 'A', 0);
}

void RunTest_Dispatcher(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      Bind(&MouseDownUp),
      TimeDelta::FromMilliseconds(100));
  DispatcherImpl dispatcher;
  RunLoop run_loop(&dispatcher);
  run_loop.Run();
  ASSERT_EQ(2, dispatcher.dispatch_count_);
}

LRESULT CALLBACK MsgFilterProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == MessagePumpForUI::kMessageFilterCode) {
    MSG* msg = reinterpret_cast<MSG*>(lparam);
    if (msg->message == WM_LBUTTONDOWN)
      return TRUE;
  }
  return FALSE;
}

void RunTest_DispatcherWithMessageHook(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      Bind(&MouseDownUp),
      TimeDelta::FromMilliseconds(100));
  HHOOK msg_hook = SetWindowsHookEx(WH_MSGFILTER,
                                    MsgFilterProc,
                                    NULL,
                                    GetCurrentThreadId());
  DispatcherImpl dispatcher;
  RunLoop run_loop(&dispatcher);
  run_loop.Run();
  ASSERT_EQ(1, dispatcher.dispatch_count_);
  UnhookWindowsHookEx(msg_hook);
}

class TestIOHandler : public MessageLoopForIO::IOHandler {
 public:
  TestIOHandler(const wchar_t* name, HANDLE signal, bool wait);

  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered, DWORD error);

  void Init();
  void WaitForIO();
  OVERLAPPED* context() { return &context_.overlapped; }
  DWORD size() { return sizeof(buffer_); }

 private:
  char buffer_[48];
  MessageLoopForIO::IOContext context_;
  HANDLE signal_;
  win::ScopedHandle file_;
  bool wait_;
};

TestIOHandler::TestIOHandler(const wchar_t* name, HANDLE signal, bool wait)
    : signal_(signal), wait_(wait) {
  memset(buffer_, 0, sizeof(buffer_));
  memset(&context_, 0, sizeof(context_));
  context_.handler = this;

  file_.Set(CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, NULL));
  EXPECT_TRUE(file_.IsValid());
}

void TestIOHandler::Init() {
  MessageLoopForIO::current()->RegisterIOHandler(file_, this);

  DWORD read;
  EXPECT_FALSE(ReadFile(file_, buffer_, size(), &read, context()));
  EXPECT_EQ(ERROR_IO_PENDING, GetLastError());
  if (wait_)
    WaitForIO();
}

void TestIOHandler::OnIOCompleted(MessageLoopForIO::IOContext* context,
                                  DWORD bytes_transfered, DWORD error) {
  ASSERT_TRUE(context == &context_);
  ASSERT_TRUE(SetEvent(signal_));
}

void TestIOHandler::WaitForIO() {
  EXPECT_TRUE(MessageLoopForIO::current()->WaitForIOCompletion(300, this));
  EXPECT_TRUE(MessageLoopForIO::current()->WaitForIOCompletion(400, this));
}

void RunTest_IOHandler() {
  win::ScopedHandle callback_called(CreateEvent(NULL, TRUE, FALSE, NULL));
  ASSERT_TRUE(callback_called.IsValid());

  const wchar_t* kPipeName = L"\\\\.\\pipe\\iohandler_pipe";
  win::ScopedHandle server(
      CreateNamedPipe(kPipeName, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  ASSERT_TRUE(server.IsValid());

  Thread thread("IOHandler test");
  Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ASSERT_TRUE(thread.StartWithOptions(options));

  MessageLoop* thread_loop = thread.message_loop();
  ASSERT_TRUE(NULL != thread_loop);

  TestIOHandler handler(kPipeName, callback_called, false);
  thread_loop->PostTask(FROM_HERE, Bind(&TestIOHandler::Init,
                                              Unretained(&handler)));
  // Make sure the thread runs and sleeps for lack of work.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));

  const char buffer[] = "Hello there!";
  DWORD written;
  EXPECT_TRUE(WriteFile(server, buffer, sizeof(buffer), &written, NULL));

  DWORD result = WaitForSingleObject(callback_called, 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

void RunTest_WaitForIO() {
  win::ScopedHandle callback1_called(
      CreateEvent(NULL, TRUE, FALSE, NULL));
  win::ScopedHandle callback2_called(
      CreateEvent(NULL, TRUE, FALSE, NULL));
  ASSERT_TRUE(callback1_called.IsValid());
  ASSERT_TRUE(callback2_called.IsValid());

  const wchar_t* kPipeName1 = L"\\\\.\\pipe\\iohandler_pipe1";
  const wchar_t* kPipeName2 = L"\\\\.\\pipe\\iohandler_pipe2";
  win::ScopedHandle server1(
      CreateNamedPipe(kPipeName1, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  win::ScopedHandle server2(
      CreateNamedPipe(kPipeName2, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  ASSERT_TRUE(server1.IsValid());
  ASSERT_TRUE(server2.IsValid());

  Thread thread("IOHandler test");
  Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ASSERT_TRUE(thread.StartWithOptions(options));

  MessageLoop* thread_loop = thread.message_loop();
  ASSERT_TRUE(NULL != thread_loop);

  TestIOHandler handler1(kPipeName1, callback1_called, false);
  TestIOHandler handler2(kPipeName2, callback2_called, true);
  thread_loop->PostTask(FROM_HERE, Bind(&TestIOHandler::Init,
                                              Unretained(&handler1)));
  // TODO(ajwong): Do we really need such long Sleeps in ths function?
  // Make sure the thread runs and sleeps for lack of work.
  TimeDelta delay = TimeDelta::FromMilliseconds(100);
  PlatformThread::Sleep(delay);
  thread_loop->PostTask(FROM_HERE, Bind(&TestIOHandler::Init,
                                              Unretained(&handler2)));
  PlatformThread::Sleep(delay);

  // At this time handler1 is waiting to be called, and the thread is waiting
  // on the Init method of handler2, filtering only handler2 callbacks.

  const char buffer[] = "Hello there!";
  DWORD written;
  EXPECT_TRUE(WriteFile(server1, buffer, sizeof(buffer), &written, NULL));
  PlatformThread::Sleep(2 * delay);
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(callback1_called, 0)) <<
      "handler1 has not been called";

  EXPECT_TRUE(WriteFile(server2, buffer, sizeof(buffer), &written, NULL));

  HANDLE objects[2] = { callback1_called.Get(), callback2_called.Get() };
  DWORD result = WaitForMultipleObjects(2, objects, TRUE, 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

#endif  // defined(OS_WIN)

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that message loops work properly in all configurations.  Of course, in some
// cases, a unit test may only be for a particular type of loop.

TEST(MessageLoopTest, PostTask) {
  RunTest_PostTask(MessageLoop::TYPE_DEFAULT);
  RunTest_PostTask(MessageLoop::TYPE_UI);
  RunTest_PostTask(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostTask_SEH) {
  RunTest_PostTask_SEH(MessageLoop::TYPE_DEFAULT);
  RunTest_PostTask_SEH(MessageLoop::TYPE_UI);
  RunTest_PostTask_SEH(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_Basic) {
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InDelayOrder) {
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder) {
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder_2) {
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder_3) {
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_SharedTimer) {
  RunTest_PostDelayedTask_SharedTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_SharedTimer(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_SharedTimer(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
TEST(MessageLoopTest, PostDelayedTask_SharedTimer_SubPump) {
  RunTest_PostDelayedTask_SharedTimer_SubPump();
}
#endif

// TODO(darin): MessageLoop does not support deleting all tasks in the
// destructor.
// Fails, http://crbug.com/50272.
TEST(MessageLoopTest, DISABLED_EnsureDeletion) {
  RunTest_EnsureDeletion(MessageLoop::TYPE_DEFAULT);
  RunTest_EnsureDeletion(MessageLoop::TYPE_UI);
  RunTest_EnsureDeletion(MessageLoop::TYPE_IO);
}

// TODO(darin): MessageLoop does not support deleting all tasks in the
// destructor.
// Fails, http://crbug.com/50272.
TEST(MessageLoopTest, DISABLED_EnsureDeletion_Chain) {
  RunTest_EnsureDeletion_Chain(MessageLoop::TYPE_DEFAULT);
  RunTest_EnsureDeletion_Chain(MessageLoop::TYPE_UI);
  RunTest_EnsureDeletion_Chain(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
TEST(MessageLoopTest, Crasher) {
  RunTest_Crasher(MessageLoop::TYPE_DEFAULT);
  RunTest_Crasher(MessageLoop::TYPE_UI);
  RunTest_Crasher(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, CrasherNasty) {
  RunTest_CrasherNasty(MessageLoop::TYPE_DEFAULT);
  RunTest_CrasherNasty(MessageLoop::TYPE_UI);
  RunTest_CrasherNasty(MessageLoop::TYPE_IO);
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, Nesting) {
  RunTest_Nesting(MessageLoop::TYPE_DEFAULT);
  RunTest_Nesting(MessageLoop::TYPE_UI);
  RunTest_Nesting(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveDenial1) {
  RunTest_RecursiveDenial1(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial1(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial1(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveDenial3) {
  RunTest_RecursiveDenial3(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial3(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial3(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveSupport1) {
  RunTest_RecursiveSupport1(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveSupport1(MessageLoop::TYPE_UI);
  RunTest_RecursiveSupport1(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
// This test occasionally hangs http://crbug.com/44567
TEST(MessageLoopTest, DISABLED_RecursiveDenial2) {
  RunTest_RecursiveDenial2(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveSupport2) {
  // This test requires a UI loop
  RunTest_RecursiveSupport2(MessageLoop::TYPE_UI);
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, NonNestableWithNoNesting) {
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_DEFAULT);
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_UI);
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, NonNestableInNestedLoop) {
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_DEFAULT, false);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_UI, false);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_IO, false);
}

TEST(MessageLoopTest, NonNestableDelayedInNestedLoop) {
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_DEFAULT, true);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_UI, true);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_IO, true);
}

TEST(MessageLoopTest, QuitNow) {
  RunTest_QuitNow(MessageLoop::TYPE_DEFAULT);
  RunTest_QuitNow(MessageLoop::TYPE_UI);
  RunTest_QuitNow(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitTop) {
  RunTest_RunLoopQuitTop(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitTop(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitTop(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitNested) {
  RunTest_RunLoopQuitNested(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitNested(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitNested(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitBogus) {
  RunTest_RunLoopQuitBogus(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitBogus(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitBogus(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitDeep) {
  RunTest_RunLoopQuitDeep(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitDeep(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitDeep(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitOrderBefore) {
  RunTest_RunLoopQuitOrderBefore(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitOrderBefore(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitOrderBefore(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitOrderDuring) {
  RunTest_RunLoopQuitOrderDuring(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitOrderDuring(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitOrderDuring(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RunLoopQuitOrderAfter) {
  RunTest_RunLoopQuitOrderAfter(MessageLoop::TYPE_DEFAULT);
  RunTest_RunLoopQuitOrderAfter(MessageLoop::TYPE_UI);
  RunTest_RunLoopQuitOrderAfter(MessageLoop::TYPE_IO);
}

class DummyTaskObserver : public MessageLoop::TaskObserver {
 public:
  explicit DummyTaskObserver(int num_tasks)
      : num_tasks_started_(0),
        num_tasks_processed_(0),
        num_tasks_(num_tasks) {}

  virtual ~DummyTaskObserver() {}

  virtual void WillProcessTask(const PendingTask& pending_task) OVERRIDE {
    num_tasks_started_++;
    EXPECT_TRUE(pending_task.time_posted != TimeTicks());
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_ + 1);
  }

  virtual void DidProcessTask(const PendingTask& pending_task) OVERRIDE {
    num_tasks_processed_++;
    EXPECT_TRUE(pending_task.time_posted != TimeTicks());
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_);
  }

  int num_tasks_started() const { return num_tasks_started_; }
  int num_tasks_processed() const { return num_tasks_processed_; }

 private:
  int num_tasks_started_;
  int num_tasks_processed_;
  const int num_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskObserver);
};

TEST(MessageLoopTest, TaskObserver) {
  const int kNumPosts = 6;
  DummyTaskObserver observer(kNumPosts);

  MessageLoop loop;
  loop.AddTaskObserver(&observer);
  loop.PostTask(FROM_HERE, Bind(&PostNTasksThenQuit, kNumPosts));
  loop.Run();
  loop.RemoveTaskObserver(&observer);

  EXPECT_EQ(kNumPosts, observer.num_tasks_started());
  EXPECT_EQ(kNumPosts, observer.num_tasks_processed());
}

#if defined(OS_WIN)
TEST(MessageLoopTest, Dispatcher) {
  // This test requires a UI loop
  RunTest_Dispatcher(MessageLoop::TYPE_UI);
}

TEST(MessageLoopTest, DispatcherWithMessageHook) {
  // This test requires a UI loop
  RunTest_DispatcherWithMessageHook(MessageLoop::TYPE_UI);
}

TEST(MessageLoopTest, IOHandler) {
  RunTest_IOHandler();
}

TEST(MessageLoopTest, WaitForIO) {
  RunTest_WaitForIO();
}

TEST(MessageLoopTest, HighResolutionTimer) {
  MessageLoop loop;

  const TimeDelta kFastTimer = TimeDelta::FromMilliseconds(5);
  const TimeDelta kSlowTimer = TimeDelta::FromMilliseconds(100);

  EXPECT_FALSE(loop.IsHighResolutionTimerEnabledForTesting());

  // Post a fast task to enable the high resolution timers.
  loop.PostDelayedTask(FROM_HERE, Bind(&PostNTasksThenQuit, 1),
                       kFastTimer);
  loop.Run();
  EXPECT_TRUE(loop.IsHighResolutionTimerEnabledForTesting());

  // Post a slow task and verify high resolution timers
  // are still enabled.
  loop.PostDelayedTask(FROM_HERE, Bind(&PostNTasksThenQuit, 1),
                       kSlowTimer);
  loop.Run();
  EXPECT_TRUE(loop.IsHighResolutionTimerEnabledForTesting());

  // Wait for a while so that high-resolution mode elapses.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(
      MessageLoop::kHighResolutionTimerModeLeaseTimeMs));

  // Post a slow task to disable the high resolution timers.
  loop.PostDelayedTask(FROM_HERE, Bind(&PostNTasksThenQuit, 1),
                       kSlowTimer);
  loop.Run();
  EXPECT_FALSE(loop.IsHighResolutionTimerEnabledForTesting());
}

#endif  // defined(OS_WIN)

#if defined(OS_POSIX) && !defined(OS_NACL)

namespace {

class QuitDelegate : public MessageLoopForIO::Watcher {
 public:
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    MessageLoop::current()->QuitWhenIdle();
  }
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    MessageLoop::current()->QuitWhenIdle();
  }
};

TEST(MessageLoopTest, FileDescriptorWatcherOutlivesMessageLoop) {
  // Simulate a MessageLoop that dies before an FileDescriptorWatcher.
  // This could happen when people use the Singleton pattern or atexit.

  // Create a file descriptor.  Doesn't need to be readable or writable,
  // as we don't need to actually get any notifications.
  // pipe() is just the easiest way to do it.
  int pipefds[2];
  int err = pipe(pipefds);
  ASSERT_EQ(0, err);
  int fd = pipefds[1];
  {
    // Arrange for controller to live longer than message loop.
    MessageLoopForIO::FileDescriptorWatcher controller;
    {
      MessageLoopForIO message_loop;

      QuitDelegate delegate;
      message_loop.WatchFileDescriptor(fd,
          true, MessageLoopForIO::WATCH_WRITE, &controller, &delegate);
      // and don't run the message loop, just destroy it.
    }
  }
  if (HANDLE_EINTR(close(pipefds[0])) < 0)
    PLOG(ERROR) << "close";
  if (HANDLE_EINTR(close(pipefds[1])) < 0)
    PLOG(ERROR) << "close";
}

TEST(MessageLoopTest, FileDescriptorWatcherDoubleStop) {
  // Verify that it's ok to call StopWatchingFileDescriptor().
  // (Errors only showed up in valgrind.)
  int pipefds[2];
  int err = pipe(pipefds);
  ASSERT_EQ(0, err);
  int fd = pipefds[1];
  {
    // Arrange for message loop to live longer than controller.
    MessageLoopForIO message_loop;
    {
      MessageLoopForIO::FileDescriptorWatcher controller;

      QuitDelegate delegate;
      message_loop.WatchFileDescriptor(fd,
          true, MessageLoopForIO::WATCH_WRITE, &controller, &delegate);
      controller.StopWatchingFileDescriptor();
    }
  }
  if (HANDLE_EINTR(close(pipefds[0])) < 0)
    PLOG(ERROR) << "close";
  if (HANDLE_EINTR(close(pipefds[1])) < 0)
    PLOG(ERROR) << "close";
}

}  // namespace

#endif  // defined(OS_POSIX) && !defined(OS_NACL)

namespace {
// Inject a test point for recording the destructor calls for Closure objects
// send to MessageLoop::PostTask(). It is awkward usage since we are trying to
// hook the actual destruction, which is not a common operation.
class DestructionObserverProbe :
  public RefCounted<DestructionObserverProbe> {
 public:
  DestructionObserverProbe(bool* task_destroyed,
                           bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {
  }
  virtual void Run() {
    // This task should never run.
    ADD_FAILURE();
  }
 private:
  friend class RefCounted<DestructionObserverProbe>;

  virtual ~DestructionObserverProbe() {
    EXPECT_FALSE(*destruction_observer_called_);
    *task_destroyed_ = true;
  }

  bool* task_destroyed_;
  bool* destruction_observer_called_;
};

class MLDestructionObserver : public MessageLoop::DestructionObserver {
 public:
  MLDestructionObserver(bool* task_destroyed, bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called),
        task_destroyed_before_message_loop_(false) {
  }
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE {
    task_destroyed_before_message_loop_ = *task_destroyed_;
    *destruction_observer_called_ = true;
  }
  bool task_destroyed_before_message_loop() const {
    return task_destroyed_before_message_loop_;
  }
 private:
  bool* task_destroyed_;
  bool* destruction_observer_called_;
  bool task_destroyed_before_message_loop_;
};

}  // namespace

TEST(MessageLoopTest, DestructionObserverTest) {
  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  MessageLoop* loop = new MessageLoop;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  bool task_destroyed = false;
  bool destruction_observer_called = false;

  MLDestructionObserver observer(&task_destroyed, &destruction_observer_called);
  loop->AddDestructionObserver(&observer);
  loop->PostDelayedTask(
      FROM_HERE,
      Bind(&DestructionObserverProbe::Run,
                 new DestructionObserverProbe(&task_destroyed,
                                              &destruction_observer_called)),
      kDelay);
  delete loop;
  EXPECT_TRUE(observer.task_destroyed_before_message_loop());
  // The task should have been destroyed when we deleted the loop.
  EXPECT_TRUE(task_destroyed);
  EXPECT_TRUE(destruction_observer_called);
}


// Verify that MessageLoop sets ThreadMainTaskRunner::current() and it
// posts tasks on that message loop.
TEST(MessageLoopTest, ThreadMainTaskRunner) {
  MessageLoop loop;

  scoped_refptr<Foo> foo(new Foo());
  std::string a("a");
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, Bind(
      &Foo::Test1ConstRef, foo.get(), a));

  // Post quit task;
  MessageLoop::current()->PostTask(FROM_HERE, Bind(
      &MessageLoop::Quit, Unretained(MessageLoop::current())));

  // Now kick things off
  MessageLoop::current()->Run();

  EXPECT_EQ(foo->test_count(), 1);
  EXPECT_EQ(foo->result(), "a");
}

TEST(MessageLoopTest, IsType) {
  MessageLoop loop(MessageLoop::TYPE_UI);
  EXPECT_TRUE(loop.IsType(MessageLoop::TYPE_UI));
  EXPECT_FALSE(loop.IsType(MessageLoop::TYPE_IO));
  EXPECT_FALSE(loop.IsType(MessageLoop::TYPE_DEFAULT));
}

TEST(MessageLoopTest, RecursivePosts) {
  // There was a bug in the MessagePumpGLib where posting tasks recursively
  // caused the message loop to hang, due to the buffer of the internal pipe
  // becoming full. Test all MessageLoop types to ensure this issue does not
  // exist in other MessagePumps.

  // On Linux, the pipe buffer size is 64KiB by default. The bug caused one
  // byte accumulated in the pipe per two posts, so we should repeat 128K
  // times to reproduce the bug.
  const int kNumTimes = 1 << 17;
  RunTest_RecursivePosts(MessageLoop::TYPE_DEFAULT, kNumTimes);
  RunTest_RecursivePosts(MessageLoop::TYPE_UI, kNumTimes);
  RunTest_RecursivePosts(MessageLoop::TYPE_IO, kNumTimes);
}

}  // namespace base
