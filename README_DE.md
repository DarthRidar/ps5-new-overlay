# PS5 Overlay (KUS Edition) 🎮⚡

🇬🇧 [English README](README.md) · 🇷🇺 [Русская версия README](README_RU.md)

**PS5 In-Game Telemetry Overlay** ist ein schlankes Echtzeit-Overlay für Leistungs- und Systemwerte auf der **PlayStation 5 (Jailbreak)**.

Das Overlay wird über jedem Spiel und dem Systemmenü `SceShellUI` angezeigt und nutzt dafür die nativen PUI-Komponenten (Mono) der PlayStation. Gesteuert und eingestellt wird es über eine eingebaute **Weboberfläche**, direkt vom Handy oder PC aus.

---

## ✨ Funktionen

- 📊 **12 Messwerte in Echtzeit**:
  1. **FPS**: Bildrate über den Flip-Zähler des Display-Controllers (`/dev/dce`), mit Software-Ersatzweg über `SceVshAppSystem`-Hooks.
  2. **CPU**: Kerntemperatur der AMD-Zen-2-CPU (`sceKernelGetCpuTemperature`).
  3. **LOAD**: Gesamte CPU-Auslastung in Prozent, berechnet aus der Laufzeit der Threads.
  4. **CCLK**: Aktueller CPU-Kerntakt in MHz (`sceKernelGetCpuCoreClock` / `sceKernelGetCpuFrequency`).
  5. **GPU**: Temperatur des RDNA-2-Grafikchips (`sceKernelGetSocSensorTemperature`, Kanal 0).
  6. **GCLK**: Tatsächlicher GFXCLK-Grafiktakt in MHz (`sceKernelGetSocClock`).
  7. **RAM**: Belegung des Arbeitsspeichers (`get_page_table_stats`).
  8. **VRAM**: Belegung des Grafikspeichers (`get_page_table_stats`).
  9. **SSD**: Temperatur der M.2-NVMe-SSD über den SoC-Sensor (`sceKernelGetSocSensorTemperature`, Kanal 2). Zeigt `N/A`, wenn der Steckplatz leer ist.
  10. **FAN**: Aktuelle Lüfterleistung in Prozent (`sceKernelGetCurrentFanDuty`).
  11. **PWR**: Leistungsaufnahme der SoC-APU in Watt (`sceKernelGetSocPowerConsumption`).
  12. **IP**: Lokale IPv4-Adresse der Konsole.

- 🌐 **Eingebaute Weboberfläche (Port 8585)**:
  * Von jedem Gerät im Heimnetz erreichbar: `http://<PS5_IP>:8585`.
  * **Live-Vorschau** des HUD im Browser.
  * **Sortieren und Filtern**: Messwerte ein- und ausschalten, Reihenfolge mit ▲ / ▼ ändern.
  * **Farben**: Eigene HEX-Farbe für jeden Messwert.
  * **Position**: Oben / Unten, Links / Mitte / Rechts, dazu feiner X/Y-Versatz.
  * **Aussehen**: Schriftgröße, Leistenhöhe, Hintergrundfarbe und Deckkraft.
  * 🛑 **Entladen im Betrieb**: Overlay sauber beenden und Speicher freigeben, ohne die Konsole neu zu starten.
  * **Automatisches Speichern**: Alle Änderungen landen in `/data/overlay/config.ini` und werden beim Start geladen.

- 🚀 **Eigenständiger Loader (alles in einer ELF)**:
  * Eine einzige Datei `ps5-overlay-kus_vX.X.X.elf`.
  * Findet `SceShellUI` automatisch, erhöht die Rechte, lädt den Payload per `ptrace` in den Prozess und startet einen Arbeits-Thread mit Mono-Anbindung (`mono_thread_attach`).
  * Wird die ELF erneut gesendet, während das Overlay schon läuft, startet **keine zweite Instanz**. Es erscheint nur die Meldung „PS5 Overlay läuft bereits!“ mit dem aktuellen Port.

---

## 🛠 Schnellstart

### 1. An die Konsole senden
Sende die ELF per Netcat oder Socat an den Port deines Payload-Loaders (z. B. 9021 oder 9020):

```bash
# Mit netcat
nc -q0 192.168.1.100 9021 < ps5-overlay-kus_v1.1.0-beta.1.elf

# Oder mit socat
socat - TCP:192.168.1.100:9021 < ps5-overlay-kus_v1.1.0-beta.1.elf
```

Nach erfolgreichem Start erscheint auf dem Bildschirm:

> **„PS5 Overlay aktiv! Web-Einstellungen: Port 8585“**

Die Meldung zeigt den Port, der tatsächlich genutzt wird.

### 2. Im Browser einstellen

```
http://<IP_DEINER_PS5>:8585
```

*(Ist Port 8585 belegt, weicht der Server automatisch auf 8282, 8989 oder 9876 aus. Die Meldung auf der Konsole nennt den richtigen Port.)*

---

## 📝 Konfigurationsdatei

Alle Einstellungen liegen auf der Konsole unter **`/data/overlay/config.ini`** und sind per FTP sichtbar.

> **Hinweis zur Technik:** Der Payload läuft im Prozess `SceShellUI`, und der hat eine eigene Sandbox. Ein normales Schreiben nach `/data/overlay` landete dort früher nur in der Sandbox. Die Datei war dann per FTP unsichtbar und nach einem Neustart von SceShellUI weg. Seit v1.1.0 übergibt der Loader dem Payload einen Zugriff auf das echte `/data/overlay`. Config, Log (`overlay.log`) und die Statusdatei `running.txt` landen jetzt im echten Dateisystem.

```ini
[overlay]
enabled = true
pos_v = top          ; top / bottom
pos_h = left         ; left / center / right
offset_x = 24
offset_y = 0
font_size = 18
bar_height = 34
bg_visible = true
bg_color = #000000
bg_opacity = 0.70
port = 8585

[metrics]
order = fps,cpu,load,cclk,gpu,gclk,ram,vram,ssd,fan,pwr,ip

[fps]
enabled = true
color = #FFEB3B

[ssd]
enabled = true
color = #00E5FF
...
```

Weitere Dateien in `/data/overlay`:

| Datei | Inhalt |
|---|---|
| `overlay.log` | Log von Loader und Payload |
| `running.txt` | `<PID> <Port>`, solange das Overlay läuft (wird beim Entladen gelöscht) |

---

## ⚙️ Selbst bauen

Benötigt wird Linux mit installiertem **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)**.

```bash
git clone https://github.com/DarthRidar/ps5-new-overlay.git
cd ps5-new-overlay
make clean && make
# Ergebnis: bin/ps5-overlay-kus_<Version>.elf
```

Ein Tag mit Bindestrich (z. B. `v1.1.0-beta.1`) erzeugt über GitHub Actions automatisch eine **Beta (Pre-Release)**.

---

## ⚠️ Sicherheitshinweis

Die Weboberfläche hat keine Anmeldung. Jedes Gerät im selben Netzwerk kann die Einstellungen ändern oder das Overlay entladen. Nutze das Overlay nur in einem vertrauenswürdigen Heimnetz.

---

## 🤝 Danksagung

* Den Entwicklern des **ps5-payload-sdk**
* Den Projekten **ProsperoMgr** und **ps5-hwinfo** für das Reverse Engineering der SoC-Sensoren
* Dem **OnionHEN**-Team für die Grundlagen der Mono-PUI-Integration
