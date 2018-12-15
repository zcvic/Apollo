/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/scheduler/policy/scheduler_classic.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "cyber/common/environment.h"
#include "cyber/common/file.h"
#include "cyber/event/perf_event_cache.h"
#include "cyber/scheduler/policy/classic_context.h"
#include "cyber/scheduler/processor.h"

namespace apollo {
namespace cyber {
namespace scheduler {

using apollo::cyber::croutine::RoutineState;
using apollo::cyber::base::ReadLockGuard;
using apollo::cyber::base::WriteLockGuard;
using apollo::cyber::common::GlobalData;
using apollo::cyber::common::GetAbsolutePath;
using apollo::cyber::common::PathExists;
using apollo::cyber::common::GetProtoFromFile;
using apollo::cyber::common::WorkRoot;
using apollo::cyber::event::PerfEventCache;
using apollo::cyber::event::SchedPerf;

SchedulerClassic::SchedulerClassic() {
  // get sched config
  std::string conf("conf/");
  conf.append(GlobalData::Instance()->ProcessGroup()).append(".conf");
  auto cfg_file = GetAbsolutePath(WorkRoot(), conf);

  apollo::cyber::proto::CyberConfig cfg;
  if (PathExists(cfg_file) && GetProtoFromFile(cfg_file, &cfg)) {
    auto& groups = cfg.scheduler_conf().classic_conf().groups();
    proc_num_ = groups[0].processor_num();
    affinity_ = groups[0].affinity();
    processor_policy_ = groups[0].processor_policy();
    processor_prio_ = groups[0].processor_prio();
    ParseCpuset(groups[0].cpuset(), &cpuset_);

    for (auto& group : groups) {
      for (auto& task : group.tasks()) {
        cr_confs_[task.name()] = task;
      }
    }
  } else {
    auto& global_conf = GlobalData::Instance()->Config();
    if (global_conf.has_scheduler_conf() &&
        global_conf.scheduler_conf().has_default_proc_num()) {
      proc_num_ = global_conf.scheduler_conf().default_proc_num();
    } else {
      proc_num_ = 2;
    }
  }

  task_pool_size_ = proc_num_;

  CreateProcessor();
}

void SchedulerClassic::CreateProcessor() {
  for (uint32_t i = 0; i < proc_num_; i++) {
    auto ctx = std::make_shared<ClassicContext>();
    pctxs_.emplace_back(ctx);

    auto proc = std::make_shared<Processor>();
    proc->BindContext(ctx);
    proc->SetAffinity(cpuset_, affinity_, i);
    proc->SetSchedPolicy(processor_policy_, processor_prio_);
    processors_.push_back(std::move(proc));
  }
}

bool SchedulerClassic::DispatchTask(const std::shared_ptr<CRoutine>& cr) {
  // we use multi-key mutex to prevent race condition
  // when del && add cr with same crid
  if (likely(id_cr_wl_.find(cr->id()) == id_cr_wl_.end())) {
    {
      std::lock_guard<std::mutex> wl_lg(cr_wl_mtx_);
      if (id_cr_wl_.find(cr->id()) == id_cr_wl_.end()) {
        id_cr_wl_[cr->id()];
      }
    }
  }
  std::lock_guard<std::mutex> lg(id_cr_wl_[cr->id()]);

  {
    WriteLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_.find(cr->id()) != id_cr_.end()) {
      return false;
    }
    id_cr_[cr->id()] = cr;
  }

  if (cr_confs_.find(cr->name()) != cr_confs_.end()) {
    cr->set_priority(cr_confs_[cr->name()].prio());
  }

  // Check if task prio is reasonable.
  if (cr->priority() >= MAX_PRIO) {
    AWARN << cr->name() << " prio is greater than MAX_PRIO[ << " << MAX_PRIO
          << "].";
    cr->set_priority(MAX_PRIO - 1);
  }

  // Enqueue task.
  {
    WriteLockGuard<AtomicRWLock> lk(ClassicContext::rq_locks_[cr->priority()]);
    ClassicContext::rq_[cr->priority()].emplace_back(cr);
  }

  PerfEventCache::Instance()->AddSchedEvent(SchedPerf::RT_CREATE, cr->id(),
                                            cr->processor_id());
  ClassicContext::Notify();
  return true;
}

bool SchedulerClassic::NotifyProcessor(uint64_t crid) {
  if (unlikely(stop_)) {
    return true;
  }

  {
    ReadLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_.find(crid) != id_cr_.end()) {
      auto cr = id_cr_[crid];
      if (cr->state() == RoutineState::DATA_WAIT) {
        cr->SetUpdateFlag();
      }

      ClassicContext::Notify();
      return true;
    }
  }
  return false;
}

bool SchedulerClassic::RemoveTask(const std::string& name) {
  if (unlikely(stop_)) {
    return true;
  }

  auto crid = GlobalData::GenerateHashId(name);
  return RemoveCRoutine(crid);
}

bool SchedulerClassic::RemoveCRoutine(uint64_t crid) {
  // we use multi-key mutex to prevent race condition
  // when del && add cr with same crid
  if (unlikely(id_cr_wl_.find(crid) == id_cr_wl_.end())) {
    {
      std::lock_guard<std::mutex> wl_lg(cr_wl_mtx_);
      if (id_cr_wl_.find(crid) == id_cr_wl_.end()) {
        id_cr_wl_[crid];
      }
    }
  }
  std::lock_guard<std::mutex> lg(id_cr_wl_[crid]);

  std::shared_ptr<CRoutine> cr = nullptr;
  int prio;
  {
    WriteLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_.find(crid) != id_cr_.end()) {
      cr = id_cr_[crid];
      prio = cr->priority();
      id_cr_[crid]->Stop();
      id_cr_.erase(crid);
    } else {
      return false;
    }
  }

  WriteLockGuard<AtomicRWLock> lk(ClassicContext::rq_locks_[prio]);
  for (auto it = ClassicContext::rq_[prio].begin();
       it != ClassicContext::rq_[prio].end(); ++it) {
    if ((*it)->id() == crid) {
      auto cr = *it;

      (*it)->Stop();
      it = ClassicContext::rq_[prio].erase(it);
      cr->Release();
      return true;
    }
  }

  return false;
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
