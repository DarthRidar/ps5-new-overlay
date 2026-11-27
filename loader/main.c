#include "proc.h"
#include "ucred.h"
#include "injector.h"
#include "notify.h"
#include "../common/log.h"
#include "../common/bootstrap.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define STATUS_PATH OVERLAY_DATA_DIR "/" OVERLAY_STATUS_NAME

// Liest die Statusdatei "<pid> <port>" des laufenden Overlays. Rueckgabe: 1 bei Erfolg.
static int read_overlay_status(int* out_pid, int* out_port) {
    char buf[64] = {0};
    int fd = open(STATUS_PATH, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    char* end = NULL;
    long pid = strtol(buf, &end, 10);
    if (!end || end == buf) return 0;
    long port = strtol(end, NULL, 10);
    if (pid <= 0) return 0;
    *out_pid = (int)pid;
    *out_port = (int)port;
    return 1;
}

extern uint8_t overlay_payload_start[];
extern uint32_t overlay_payload_size;

int main(int argc, char const* argv[]) {
    (void)argc;
    (void)argv;

    overlay_log_init();
    LOG_INFO("==================================================");
    LOG_INFO("PS5 Overlay Loader Started");
    LOG_INFO("==================================================");
    LOG_INFO("Embedded payload size: %u bytes", overlay_payload_size);

    pid_t self = getpid();
    LOG_INFO("Current loader PID: %d. Escalating privileges...", self);

    jailbreak_process(self);
    LOG_INFO("jailbreak_process completed for PID: %d", self);

    uint64_t orig_authid = set_ucred_to_ptrace();
    LOG_INFO("set_ucred_to_ptrace original_authid: 0x%lx", (unsigned long)orig_authid);

    LOG_INFO("Searching for SceShellUI process...");
    pid_t shellui_pid = find_process_by_name("SceShellUI");
    if (shellui_pid <= 0) {
        LOG_ERROR("Could not find SceShellUI process!");
        restore_authid(orig_authid);
        notify_send("Overlay-Fehler: SceShellUI nicht gefunden!");
        return -1;
    }

    // Laeuft das Overlay schon in genau diesem SceShellUI-Prozess? Dann nicht erneut
    // injizieren (sonst laufen zwei Instanzen parallel und der Port weicht aus).
    {
        int run_pid = 0, run_port = 0;
        if (read_overlay_status(&run_pid, &run_port) && run_pid == shellui_pid) {
            LOG_WARN("Overlay laeuft bereits in SceShellUI (PID %d, Port %d). Kein erneuter Inject.", run_pid, run_port);
            restore_authid(orig_authid);
            notify_send("PS5 Overlay läuft bereits!\nWeb-Einstellungen: Port %d", run_port);
            return 0;
        }
        // Alte Statusdatei (z. B. nach Neustart von SceShellUI) entfernen
        unlink(STATUS_PATH);
    }

    LOG_INFO("Found SceShellUI PID: %d. Injecting overlay payload...", shellui_pid);
    if (!inject_elf_into_pid(shellui_pid, overlay_payload_start)) {
        LOG_ERROR("Failed to inject overlay payload into SceShellUI!");
        restore_authid(orig_authid);
        notify_send("Overlay-Fehler: Injektion fehlgeschlagen!\nSiehe /data/overlay/overlay.log");
        return -1;
    }

    LOG_INFO("Overlay successfully injected into SceShellUI!");
    restore_authid(orig_authid);

    // Give SceShellUI threads 300ms to settle after ptrace detach before sending notification IPC
    usleep(300000);
    // Auf den tatsaechlich belegten Web-Port warten (Payload schreibt ihn nach dem bind).
    int run_pid = 0, run_port = 0;
    for (int i = 0; i < 100; i++) { // max. ca. 10 Sekunden
        if (read_overlay_status(&run_pid, &run_port) && run_pid == shellui_pid && run_port > 0) break;
        run_port = 0;
        usleep(100000);
    }

    if (run_port > 0) {
        LOG_INFO("Web-Einstellungen laufen auf Port %d", run_port);
        notify_send("PS5 Overlay aktiv!\nWeb-Einstellungen: Port %d", run_port);
    } else {
        LOG_WARN("Kein Web-Port gemeldet (Statusdatei fehlt). Siehe Log.");
        notify_send("PS5 Overlay aktiv!\nWeb-Port unbekannt, siehe /data/overlay/overlay.log");
    }

    return 0;
}
