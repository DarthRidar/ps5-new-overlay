#pragma once

#include <stdint.h>

// Gemeinsame Pfade (echtes Dateisystem der Konsole)
#define OVERLAY_DATA_DIR     "/data/overlay"
#define OVERLAY_CONFIG_NAME  "config.ini"
#define OVERLAY_STATUS_NAME  "running.txt"   // Inhalt: "<pid> <port>\n", solange das Overlay laeuft
#define OVERLAY_LOG_NAME     "overlay.log"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t getpid_addr;
    uint64_t dlsym_addr;
    uint64_t debug_out_addr;
    uint64_t pthread_create_addr;

    uint32_t libkernel_handle;
    uint64_t libkernel_mapbase;

    uint32_t libkernelsys_handle;
    uint64_t libkernelsys_mapbase;

    uint32_t libmono_handle;
    uint64_t libmono_mapbase;

    uint32_t libnetctl_handle;
    uint64_t libnetctl_mapbase;

    uint64_t mprotect_addr;

    // Pre-resolved Telemetry symbols
    uint64_t sceKernelGetCpuTemperature;
    uint64_t sceKernelGetSocSensorTemperature;
    uint64_t get_page_table_stats;
    uint64_t sceKernelGetCpuUsage;
    uint64_t sceKernelGetThreadName;
    uint64_t sceKernelClockGettime;
    uint64_t sceKernelGetCurrentFanDuty;
    uint64_t sceNetCtlGetInfo;
    uint64_t sceKernelGetSocClock;
    uint64_t sceKernelGetSocPowerConsumption;
    uint64_t sceKernelGetCpuCoreClock;
    uint64_t sceKernelGetCpuFrequency;

    // Pre-resolved Mono runtime symbols
    uint64_t mono_get_root_domain;
    uint64_t mono_domain_get;
    uint64_t mono_domain_assembly_open;
    uint64_t mono_assembly_get_image;
    uint64_t mono_class_from_name;
    uint64_t mono_class_get_method_from_name;
    uint64_t mono_class_get_property_from_name;
    uint64_t mono_property_get_get_method;
    uint64_t mono_property_get_set_method;
    uint64_t mono_compile_method;
    uint64_t mono_runtime_invoke;
    uint64_t mono_runtime_object_init;
    uint64_t mono_object_new;
    uint64_t mono_object_unbox;
    uint64_t mono_string_new;
    uint64_t mono_string_to_utf8;
    uint64_t mono_thread_attach;
    uint64_t mono_thread_current;
    uint64_t mono_thread_detach;

    // Verzeichnis-Deskriptor fuer das echte /data/overlay (vom Loader per rdup in SceShellUI
    // eingeschleust). SceShellUI laeuft in einer Sandbox, in der "/data" NICHT das echte
    // /data der Konsole ist. Ueber diesen Deskriptor (openat) landen Config, Log und
    // Statusdatei dort, wo man sie per FTP sieht. -1 = nicht verfuegbar.
    int32_t data_dirfd;
} PayloadBootstrapArgs;

#ifdef __cplusplus
}
#endif

