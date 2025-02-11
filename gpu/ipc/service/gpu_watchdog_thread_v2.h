// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_

#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuWatchdogThreadImplV2
    : public GpuWatchdogThread,
      public base::MessageLoopCurrent::TaskObserver {
 public:
  static std::unique_ptr<GpuWatchdogThreadImplV2> Create(
      bool start_backgrounded);

  static std::unique_ptr<GpuWatchdogThreadImplV2>
  Create(bool start_backgrounded, base::TimeDelta timeout, bool test_mode);

  ~GpuWatchdogThreadImplV2() override;

  // Implements GpuWatchdogThread.
  void AddPowerObserver() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
  void OnInitComplete() override;
  void GpuWatchdogHistogram(GpuWatchdogThreadEvent thread_event) override;
  bool IsGpuHangDetectedForTesting() override;
  void WaitForPowerObserverAddedForTesting() override;

  // Implements base::Thread.
  void Init() override;
  void CleanUp() override;

  // Implements gl::ProgressReporter.
  void ReportProgress() override;

  // Implements MessageLoopCurrent::TaskObserver.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // Implements base::PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

 private:
  GpuWatchdogThreadImplV2(base::TimeDelta timeout, bool test_mode);
  void OnAddPowerObserver();
  void OnWatchdogBackgrounded();
  void OnWatchdogForegrounded();
  void RestartWatchdogTimeoutTask();
  void Arm();
  void Disarm();
  void InProgress();
  void OnWatchdogTimeout();

  // Do not change the function name. It is used for [GPU HANG] carsh reports.
  void DeliberatelyTerminateToRecoverFromHang();

  // This counter is only written on the gpu thread, and read on the watchdog
  // thread.
  base::subtle::Atomic32 arm_disarm_counter_ = 0;
  // The counter number read in the last OnWatchdogTimeout() on the watchdog
  // thread.
  int32_t last_arm_disarm_counter_ = 0;

  // Timeout on the watchdog thread to check if gpu hangs
  base::TimeDelta watchdog_timeout_;

  // The time the gpu watchdog was created
  base::TimeTicks watchdog_start_time_;

  // The time the last OnSuspend and OnResume was called.
  base::TimeTicks suspend_time_;
  base::TimeTicks resume_time_;

  // The time the last OnBackgrounded and OnForegrounded was called.
  base::TimeTicks backgrounded_time_;
  base::TimeTicks foregrounded_time_;

  // The system has entered the power suspension mode.
  bool in_power_suspension_ = false;

  // Chrome is running on the background on Android. Gpu is probably very slow
  // or stalled.
  bool is_backgrounded_ = false;

  // Whether the watchdog thread has been called and added to the power monitor
  // observer.
  bool is_add_power_observer_called_ = false;
  bool is_power_observer_added_ = false;

  // For gpu testing only.
  const bool is_test_mode_;
  // Set by the watchdog thread and Read by the test thread.
  base::AtomicFlag test_result_timeout_and_gpu_hang_;

  scoped_refptr<base::SingleThreadTaskRunner> watched_task_runner_;

  base::WeakPtr<GpuWatchdogThreadImplV2> weak_ptr_;
  base::WeakPtrFactory<GpuWatchdogThreadImplV2> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThreadImplV2);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_
