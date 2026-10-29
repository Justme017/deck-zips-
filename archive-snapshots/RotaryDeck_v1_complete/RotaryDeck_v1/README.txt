ROTARY DECK v1.0 — CONSOLIDATED SOURCE DELIVERY

STATUS
Source implementation, NOT hardware validated and NOT compiled in this environment.
This replaces all previous UI headers/adapters. Do NOT add RotaryDeckUI.h.
No binary firmware is supplied. Physical LCD, BLE, encoder and wake behaviour
must be validated on your board. Source review is not a compiler test.

TARGET DEPENDENCIES
Arduino-ESP32 by Espressif: 2.0.17 (chosen to avoid the original BleKeyboard
library's Arduino-ESP32 3.x string API incompatibility).
ESP32 BLE Keyboard by T-vK: 0.3.0, standard Bluedroid backend.
U8g2 by olikraus: 2.x. Preferences is included in the ESP32 core.
This is an explicit library target, NOT a claim that your installed BLE library
has been identified. Do not use USE_NIMBLE for this source.

INSTALL
Extract the folder. Open RotaryDeck_v1.ino with DeckFirmware.h beside it.
Install the target dependencies. Select XIAO_ESP32S3 and the connected port.
Enable USB CDC On Boot for USB serial; default board settings otherwise.
Compile, then upload. Old RotaryDeckUI.h is no longer needed.

WIRING: EXP3 PIN -> XIAO PIN -> GPIO
1 +5V -> VBUS/5V; 2 GND -> GND; 3 SID -> D10/GPIO9;
4 CS -> D7/GPIO44; 5 SCK -> D8/GPIO7; 6 Encoder B -> D3/GPIO4;
7 disconnected; 8 Encoder A -> D0/GPIO1; 9 Button -> D4/GPIO5;
10 Beeper -> D5/GPIO6.
LCD assumes ST7920 128x64 serial mode. Pin map alone does not prove controller.
If your previously working constructor differs, use that constructor instead.
Encoder/button assumed dry contacts to ground. Inspect for 5V pull-ups: ESP32
GPIOs must NOT be driven to 5V. Verify LCD recognizes 3.3V output logic.
On XIAO Sense, do not initialize SD on the shared SPI pins.

PAIRING
Look for Rotary Deck in Windows Bluetooth Add device.
Bluetooth icon: solid=connected; blink=advertising requested; slash=advertising
stopped and disconnected. Discoverable toggle applies only while disconnected.
Disconnected library callbacks normally restart advertising. Firmware reapplies
user advertising preference after disconnect. Settings > Connection > Forget
pairing requests deletion of local BLE bonds and restarts. Also remove the
old device in Windows before pairing again. Asynchronous bond-removal completion
needs hardware verification; an 800ms restart delay is used.
The library advertises a nominal battery value of 100; no battery sensor exists.

CONTROLS
Turn: navigate. Press: select/send. Hold 700ms: back. Double press unassigned.
Media > Volume: press to enter adjustment, turn to send volume commands,
press to finish. Hold exits adjustment. Volume commands queue with 45ms spacing.
The Volume command step setting is number of HID commands, NOT a Windows
percentage increment. The first input wakes a timed-out screen without acting.
Changing encoder direction applies immediately and persists.

SETTINGS
Connection: actual connected state, advertising toggle, local bond clearing.
Display: screen timeout (never/30/60/120 seconds). Timeout blanks pixels only.
Controls: commands per detent, encoder direction, 2/4 transitions per detent,
button sound. About: firmware/device info. Turn off: confirmed shutdown.
Settings persist using Preferences. Edit press saves; hold cancels.
No unsupported brightness control is exposed.
Beeper sound defaults OFF; optional short software pulse train may require
adjustment for your beeper type. Direct beeper driving assumes the LCD PCB
contains a suitable input/driver circuit; verify before enabling sound.

WINDOWS APPS
Files uses Win+E. Other app tiles send Ctrl+Alt+Shift+F1/F2/F3/F4/F6.
Install AutoHotkey v2, REVIEW WindowsApps.ahk, edit paths to match installed apps,
then run it normally (not as administrator). Do not enable startup automatically
until tested. Hotkeys are global. Microsoft Store app installs may need different
launch commands. This helper is optional: media/actions work without it.
The display says Shortcut sent, not App opened; no PC feedback exists.
Actions: Snipping tool Win+Shift+S, mute media key, lock Win+L, search Win+S,
show desktop Win+D, task view Win+Tab. Results depend on Windows/app settings.

POWER
Turn off saves settings, animates, stops advertising, blanks LCD, and deep sleeps.
Press encoder to wake. Boot inhibits the held wake press until button release.
5V/backlight stays physically supplied; no supply-cutoff hardware is present.
Deep sleep is NOT zero-power shutdown. Wake restarts BLE and the boot animation.
Screen timeout is NOT deep sleep.

ACCEPTANCE CHECKLIST
[ ] Compile with the target versions; retain complete errors if unsuccessful.
[ ] LCD readable, correct orientation, no blank/mirrored output.
[ ] One detent = one item; adjust 2/4 transitions and direction in Controls.
[ ] Click executes once; hold returns without extra click.
[ ] Windows pairs; solid Bluetooth icon; reconnect after host Bluetooth toggled.
[ ] Media keys work, volume command step appropriate.
[ ] All six Actions work; do not test Lock with unsaved work.
[ ] App helper paths reviewed, shortcuts launch the intended apps.
[ ] Settings survive reset; edit cancellation does not save changes.
[ ] Timeout consumes first wake input.
[ ] Forget pairing + remove on PC allows fresh pairing.
[ ] Turn off animation, button wake and no accidental action after wake.
[ ] Verify backlight remains powered as documented.

KNOWN LIMITATIONS
No Windows volume percentage, playback state, song data or host metrics.
No complete electrical power cutoff/brightness adjustment from current wiring.
Six configured apps (one page); no fake second page or undefined More tile.
BLE library/core compatibility, reconnect, bonds, electrical levels and animations
still require actual compilation and board tests. Firmware timing is cooperative:
BLE writes and boot initialization can briefly block; screen animations use millis.
