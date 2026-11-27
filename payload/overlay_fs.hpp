#pragma once
// Dateizugriff auf das ECHTE /data/overlay der Konsole.
//
// Der Payload laeuft in SceShellUI, und SceShellUI hat ein eigenes (Sandbox-)Root-
// Verzeichnis. Ein normales open("/data/overlay/config.ini") landet deshalb in der
// Sandbox und ist nach einem Neustart von SceShellUI verschwunden bzw. per FTP nicht
// sichtbar. Der Loader uebergibt einen offenen Deskriptor auf das echte /data/overlay
// (g_bargs.data_dirfd); diese Helfer benutzen ihn per openat()/unlinkat().
// Ohne Deskriptor wird auf den bisherigen absoluten Pfad zurueckgefallen.

#include <cstddef>

// true, wenn der Deskriptor auf das echte /data/overlay verfuegbar ist
bool overlay_fs_has_real_dir();

// Oeffnet <name> im echten /data/overlay (oder fallback_path). Rueckgabe < 0 bei Fehler.
long overlay_fs_open(const char* name, const char* fallback_path, int flags, int mode);

// Loescht <name> im echten /data/overlay (oder fallback_path).
void overlay_fs_unlink(const char* name, const char* fallback_path);

// Schreibt die Statusdatei "<pid> <port>" (Loader erkennt daran ein laufendes Overlay).
void overlay_fs_write_status(int port);

// Entfernt die Statusdatei und schliesst den Verzeichnis-Deskriptor.
void overlay_fs_shutdown();
