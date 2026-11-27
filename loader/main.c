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
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

// Prueft, ob der Web-Server des Overlays wirklich auf 127.0.0.1:<port> antwortt.
// Nur dann laeuft das Overlay tatsaechlich noch. So wird eine veraltete Statusdatei
// (Overlay abgestuerzt, Szene gewechselt, SceShellUI behaelt nach Unload dieselbe PID)
// nicht faelschlich als "laeuft" gewertet und blockiert kein erneutes Laden.
// Rueckgabe: 1 = Port antwortet (Overlay lebt), 0 = keine Verbindung.
static int overlay_web_alive(int port) {
    if (port < 1 || port > 65535) return 0;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 0;

    // Nicht-blockierend verbinden, damit ein fehlender Server nicht lange haengt
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    int alive = 0;
    int rc = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (rc == 0) {
        alive = 1; // sofort verbunden
    } else if (errno == EINPROGRESS) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(s, &wset);
        struct timeval tv = { 0, 500000 }; // 500 ms
        if (select(s + 1, NULL, &wset, NULL, &tv) > 0 && FD_ISSET(s, &wset)) {
            int soerr = 0;
            socklen_t slen = sizeof(soerr);
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &slen) == 0 && soerr == 0) {
                alive = 1;
            }
        }
    }
    close(s);
    return alive;
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
    // Wichtig: Die Statusdatei allein reicht als Beweis nicht aus. SceShellUI behaelt
    // ueber lange Zeit dieselbe PID; nach einem Absturz des Overlays, einem Szenen-
    // wechsel oder einem Unload, bei dem die Datei nicht geloescht wurde, waere die
    // Sperre sonst dauerhaft und man koennte nie wieder laden. Deshalb wird zusaetzlich
    // geprueft, ob der Web-Server wirklich antwortet.
    {
        int run_pid = 0, run_port = 0;
        if (read_overlay_status(&run_pid, &run_port) && run_pid == shellui_pid
                && overlay_web_alive(run_port)) {
            LOG_WARN("Overlay laeuft bereits in SceShellUI (PID %d, Port %d, Web-Server antwortet). Kein erneuter Inject.", run_pid, run_port);
            restore_authid(orig_authid);
            notify_send("PS5 Overlay läuft bereits!\nWeb-Einstellungen: Port %d", run_port);
            return 0;
        }
        // Keine laufende Instanz (oder veraltete Statusdatei): alten Stand entfernen
        // und normal injizieren.
        if (run_pid != 0) {
            LOG_INFO("Veraltete Statusdatei gefunden (PID %d, Port %d, kein antwortender Web-Server). Wird verworfen, neuer Inject.", run_pid, run_port);
        }
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
