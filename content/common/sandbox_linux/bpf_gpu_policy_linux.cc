// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if !defined(__arm__)
// Auto-generated for dlopen libva libraries
#include "content/common/va_stubs.h"
#endif

#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "content/common/set_process_title.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if !defined(__arm__)
#include "third_party/libva/va/va.h"
#include "third_party/libva/va/va_backend.h"
#if defined(USE_X11)
#include "third_party/libva/va/va_x11.h"
#include "ui/gfx/x/x11_types.h"
#elif defined(USE_OZONE)
#include "third_party/libva/va/va_drm.h"
#include "third_party/libva/va/va_drmcommon.h"
#endif
#endif

#define DRIVER_EXTENSION        "_drv_video.so"
#if defined(__x86_64__)
#define VA_DRIVERS_PATH         "/usr/lib/x86_64-linux-gnu/dri"
#elif defined(__i386__)
#define VA_DRIVERS_PATH         "/usr/lib/i386-linux-gnu/dri"
#elif defined(__aarch64__)
#define VA_DRIVERS_PATH         "/usr/lib/aarch64-linux-gnu/dri"
#endif

using sandbox::arch_seccomp_data;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;
using sandbox::SyscallSets;

#if !defined(__arm__)
using content_common::kModuleVa;
#if defined(USE_X11)
using content_common::kModuleVa_x11;
#elif defined(USE_OZONE)
using content_common::kModuleVa_drm;
#endif
using content_common::InitializeStubs;
using content_common::StubPathMap;
#endif

namespace content {

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(__arm__) || defined(__aarch64__)
  return true;
#else
  return false;
#endif
}

inline bool UseV4L2Codec() {
#if defined(USE_V4L2_CODEC)
  return true;
#else
  return false;
#endif
}

inline bool UseLibV4L2() {
#if defined(USE_LIBV4L2)
  return true;
#else
  return false;
#endif
}

bool IsAcceleratedVaapiVideoEncodeEnabled() {
  bool accelerated_encode_enabled = false;
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  accelerated_encode_enabled =
      !command_line.HasSwitch(switches::kDisableVaapiAcceleratedVideoEncode);
#endif
  return accelerated_encode_enabled;
}

bool IsAcceleratedVideoDecodeEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);
}

intptr_t GpuSIGSYS_Handler(const struct arch_seccomp_data& args,
                           void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
#if !defined(__aarch64__)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
#if defined(MEMORY_SANITIZER)
      // http://crbug.com/372840
      __msan_unpoison_string(reinterpret_cast<const char*>(args.args[0]));
#endif
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Access(
            reinterpret_cast<const char*>(args.args[1]),
            static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

void AddV4L2GpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  if (IsAcceleratedVideoDecodeEnabled()) {
    // Device node for V4L2 video decode accelerator drivers.
    static const char kDevVideoDecPath[] = "/dev/video-dec";
    permissions->push_back(BrokerFilePermission::ReadWrite(kDevVideoDecPath));
  }

  // Device node for V4L2 video encode accelerator drivers.
  static const char kDevVideoEncPath[] = "/dev/video-enc";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevVideoEncPath));

  // Device node for V4L2 JPEG decode accelerator drivers.
  static const char kDevJpegDecPath[] = "/dev/jpeg-dec";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegDecPath));
}

class GpuBrokerProcessPolicy : public GpuProcessPolicy {
 public:
  static sandbox::bpf_dsl::Policy* Create() {
    return new GpuBrokerProcessPolicy();
  }
  ~GpuBrokerProcessPolicy() override {}

  ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  GpuBrokerProcessPolicy() {}
  DISALLOW_COPY_AND_ASSIGN(GpuBrokerProcessPolicy);
};

// x86_64/i386 or desktop ARM.
// A GPU broker policy is the same as a GPU policy with access, open,
// openat and in the non-Chrome OS case unlink allowed.
ResultExpr GpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
#if !defined(OS_CHROMEOS) && !defined(__aarch64__)
    // The broker process needs to able to unlink the temporary
    // files that it may create. This is used by DRI3.
    case __NR_unlink:
#endif
      return Allow();
    default:
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

void UpdateProcessTypeToGpuBroker() {
  base::CommandLine::StringVector exec =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  base::CommandLine::Reset();
  base::CommandLine::Init(0, NULL);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(exec);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, "gpu-broker");

  // Update the process title. The argv was already cached by the call to
  // SetProcessTitleFromCommandLine in content_main_runner.cc, so we can pass
  // NULL here (we don't have the original argv at this point).
  SetProcessTitleFromCommandLine(NULL);
}

bool UpdateProcessTypeAndEnableSandbox(
    sandbox::bpf_dsl::Policy* (*broker_sandboxer_allocator)(void)) {
  DCHECK(broker_sandboxer_allocator);
  UpdateProcessTypeToGpuBroker();
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      base::WrapUnique(broker_sandboxer_allocator()), base::ScopedFD());
}

}  // namespace

GpuProcessPolicy::GpuProcessPolicy() : GpuProcessPolicy(false) {}

GpuProcessPolicy::GpuProcessPolicy(bool allow_mincore)
    : broker_process_(NULL), allow_mincore_(allow_mincore) {}

GpuProcessPolicy::~GpuProcessPolicy() {}

// Main policy for x86_64/i386. Extended by CrosArmGpuProcessPolicy.
ResultExpr GpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(OS_CHROMEOS)
    case __NR_ftruncate:
#endif
    case __NR_ioctl:
      return Allow();
    case __NR_mincore:
      if (allow_mincore_) {
        return Allow();
      } else {
        return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
      }
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    // The Nvidia driver uses flags not in the baseline policy
    // (MAP_LOCKED | MAP_EXECUTABLE | MAP_32BIT)
    case __NR_mmap:
#endif
    // We also hit this on the linux_chromeos bot but don't yet know what
    // weird flags were involved.
    case __NR_mprotect:
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_sysinfo:
      return Allow();
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
      DCHECK(broker_process_);
      return Trap(GpuSIGSYS_Handler, broker_process_);
    case __NR_sched_getaffinity:
    case __NR_sched_setaffinity:
      return sandbox::RestrictSchedTarget(GetPolicyPid(), sysno);
    default:
      if (SyscallSets::IsEventFd(sysno))
        return Allow();

      // Default on the baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
  }
}

#if !defined(__arm__)
static VADisplay VaapiGetVADisplay() {

  StubPathMap paths;
  VADisplay va_display;
  paths[kModuleVa].push_back("libva.so.1");
#if defined(USE_OZONE)
  paths[kModuleVa_drm].push_back("libva-drm.so.1");
#elif defined (USE_X11)
  paths[kModuleVa_x11].push_back("libva-x11.so.1");
#endif
  if (!InitializeStubs(paths)) {
    return NULL;
  }
#if defined(USE_OZONE)
  va_display = vaGetDisplayDRM(drm_fd_.get());
#elif defined(USE_X11)
  if (content_common::IsVa_x11Initialized()) {
    va_display = vaGetDisplay(gfx::GetXDisplay());
  }
  else {
    LOG(ERROR) << "va initialize error";
    return NULL;
  }
#endif
  if (!vaDisplayIsValid(va_display)) {
    LOG(WARNING) << "Unable to get display.";
    return NULL;
  }
  return va_display;
}
#endif

bool GpuProcessPolicy::PreSandboxHook() {
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  const bool chromeos_arm_gpu = IsChromeOS() && IsArchitectureArm();
  // This policy is for x86 or Desktop.
  DCHECK(!chromeos_arm_gpu);

  DCHECK(!broker_process());
  // Create a new broker process.
  InitGpuBrokerProcess(
      GpuBrokerProcessPolicy::Create,
      std::vector<BrokerFilePermission>());  // No extra files in whitelist.

#if !defined(__arm__)
  if (IsArchitectureX86_64() || IsArchitectureI386()) {
    // Accelerated video dlopen()'s some shared objects
    // inside the sandbox, so preload them now.
    if (IsAcceleratedVaapiVideoEncodeEnabled() ||
        IsAcceleratedVideoDecodeEnabled()) {
      // const char* I965DrvVideoPath = NULL;
      // const char* I965HybridDrvVideoPath = NULL;

      // if (IsArchitectureX86_64()) {
      //   I965DrvVideoPath = "/usr/lib64/va/drivers/i965_drv_video.so";
      //   I965HybridDrvVideoPath = "/usr/lib64/va/drivers/hybrid_drv_video.so";
      // } else if (IsArchitectureI386()) {
      //   I965DrvVideoPath = "/usr/lib/va/drivers/i965_drv_video.so";
      // }

      // dlopen(I965DrvVideoPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      // if (I965HybridDrvVideoPath)
      //   dlopen(I965HybridDrvVideoPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      // dlopen("libva.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);

      // Init a VA Context, get driver name (and path), close context and preload
#if defined(USE_OZONE) || defined(USE_X11)
      VADisplay va_display = VaapiGetVADisplay();
      if (va_display) {
        char *va_search_path = getenv("LIBVA_DRIVERS_PATH");
        char *va_driver_name = getenv("LIBVA_DRIVER_NAME");

        char *saveptr, *driver_dir;
        if (!va_driver_name) {
          VADisplayContextP pDisplayContext =
            static_cast<VADisplayContextP>(va_display);
          pDisplayContext->vaGetDriverName(pDisplayContext, &va_driver_name);
        }

        if (!va_driver_name) {
          return true;
        }

        if (!va_search_path) {
          va_search_path = strdup(VA_DRIVERS_PATH);
        }

        driver_dir = strtok_r(va_search_path, ":", &saveptr);
        while (driver_dir) {
          std::string driver_path = base::StringPrintf("%s/%s%s", driver_dir,
              va_driver_name, DRIVER_EXTENSION);

          // attempt to dlopen() the va driver
          LOG(INFO) << "Looking for " << driver_path << "...";
          if (access(driver_path.c_str(), R_OK) != -1) {
            LOG(WARNING) << "Attempting to pre-load " << va_driver_name << " VA driver...";
            if (dlopen(driver_path.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
              LOG(WARNING) << "VA driver \"" << driver_path << "\" opened!";

              if (strcmp(va_driver_name, "nvidia") == 0) {
                LOG(WARNING) << "Pre-loading NVIDIA VDPAU driver libraries...";
                if (dlopen("libvdpau_nvidia.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
                  LOG(WARNING) << "Loaded libvdpau_nvidia.so";
                }

                if (dlopen("libvdpau.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
                  LOG(WARNING) << "Loaded libvdpau.so";
                }
              }
              LOG(WARNING) << "Loading libva libraries...";
              dlopen("libva.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);

 #if defined(USE_OZONE)
     // dlopen("libva-drm.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    dlopen("libva-drm.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#elif defined(USE_X11)
      //dlopen("libva-x11.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    dlopen("libva-x11.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#endif
              break;
            }
            LOG(ERROR) << "VA driver failed to open!";
          }

          driver_dir = strtok_r(NULL, ":", &saveptr);
        }
        free(va_search_path);
      }
#endif
    }
  }
  #endif

  return true;
}

void GpuProcessPolicy::InitGpuBrokerProcess(
    sandbox::bpf_dsl::Policy* (*broker_sandboxer_allocator)(void),
    const std::vector<BrokerFilePermission>& permissions_extra) {
  static const char kDriRcPath[] = "/etc/drirc";
  static const char kDriCard0Path[] = "/dev/dri/card0";
  static const char kDriCardBasePath[] = "/dev/dri/card";

  static const char kNvidiaCtlPath[] = "/dev/nvidiactl";
  static const char kNvidiaDeviceBasePath[] = "/dev/nvidia";
  static const char kNvidiaParamsPath[] = "/proc/driver/nvidia/params";

  static const char kDevShm[] = "/dev/shm/";

  CHECK(broker_process_ == NULL);

  // All GPU process policies need these files brokered out.
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadWrite(kDriCard0Path));
  permissions.push_back(BrokerFilePermission::ReadOnly(kDriRcPath));

  if (!IsChromeOS()) {
    // For shared memory.
    permissions.push_back(
        BrokerFilePermission::ReadWriteCreateUnlinkRecursive(kDevShm));
    // For multi-card DRI setups. NOTE: /dev/dri/card0 was already added above.
    for (int i = 1; i <= 9; ++i) {
      permissions.push_back(BrokerFilePermission::ReadWrite(
          base::StringPrintf("%s%d", kDriCardBasePath, i)));
    }
    // For Nvidia GLX driver.
    permissions.push_back(BrokerFilePermission::ReadWrite(kNvidiaCtlPath));
    for (int i = 0; i <= 9; ++i) {
      permissions.push_back(BrokerFilePermission::ReadWrite(
          base::StringPrintf("%s%d", kNvidiaDeviceBasePath, i)));
    }
    permissions.push_back(BrokerFilePermission::ReadOnly(kNvidiaParamsPath));
  } else if (UseV4L2Codec()) {
    AddV4L2GpuWhitelist(&permissions);
    if (UseLibV4L2()) {
      dlopen("/usr/lib/libv4l2.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      // This is a device-specific encoder plugin.
      dlopen("/usr/lib/libv4l/plugins/libv4l-encplugin.so",
             RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    }
  }

  // Add eventual extra files from permissions_extra.
  for (const auto& perm : permissions_extra) {
    permissions.push_back(perm);
  }

  broker_process_ = new BrokerProcess(GetFSDeniedErrno(), permissions);
  // The initialization callback will perform generic initialization and then
  // call broker_sandboxer_callback.
  CHECK(broker_process_->Init(base::Bind(&UpdateProcessTypeAndEnableSandbox,
                                         broker_sandboxer_allocator)));
}

}  // namespace content
