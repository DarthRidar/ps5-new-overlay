#include "overlay_fs.hpp"
#include "../common/bootstrap.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern "C" long __syscall(long sysno, ...);
extern PayloadBootstrapArgs g_bargs;

#define SYS_write    4
#define SYS_open     5
#define SYS_close    6
#define SYS_unlink   10
#define SYS_fsync    95
#define SYS_openat   499
#define SYS_unlinkat 503

#define O_WRONLY 0x0001
#define O_CREAT  0x0200
#define O_TRUNC  0x0400

static volatile bool s_dir_closed = false;

bool overlay_fs_has_real_dir() {
    // g_bargs ist vor dem Kopieren der Bootstrap-Argumente mit 0 gefuellt; fd 0 (stdin)
    // ist daher nie ein gueltiger Verzeichnis-Deskriptor.
    return !s_dir_closed && g_bargs.data_dirfd > 0;
}

long overlay_fs_open(const char* name, const char* fallback_path, int flags, int mode) {
    if (overlay_fs_has_real_dir() && name) {
        long fd = __syscall(SYS_openat, (long)g_bargs.data_dirfd, name, flags, mode);
        if (fd >= 0) return fd;
    }
    if (!fallback_path) return -1;
    return __syscall(SYS_open, fallback_path, flags, mode);
}

void overlay_fs_unlink(const char* name, const char* fallback_path) {
    if (overlay_fs_has_real_dir() && name) {
        __syscall(SYS_unlinkat, (long)g_bargs.data_dirfd, name, 0);
    }
    if (fallback_path) {
        __syscall(SYS_unlink, fallback_path);
    }
}

void overlay_fs_write_status(int port) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d %d\n", (int)getpid(), port);
    if (len <= 0) return;
    long fd = overlay_fs_open(OVERLAY_STATUS_NAME, "/system_tmp/ps5_overlay_running.txt",
                              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        __syscall(SYS_write, fd, buf, (long)len);
        __syscall(SYS_fsync, fd);
        __syscall(SYS_close, fd);
    }
}

void overlay_fs_shutdown() {
    overlay_fs_unlink(OVERLAY_STATUS_NAME, "/system_tmp/ps5_overlay_running.txt");
    if (g_bargs.data_dirfd > 0 && !s_dir_closed) {
        s_dir_closed = true;
        __syscall(SYS_close, (long)g_bargs.data_dirfd);
    }
}
