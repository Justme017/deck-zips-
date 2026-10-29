# RotaryDeck v2.2 volume-sync test candidate

Built from the five files you uploaded. This is a matched firmware/UI/companion update, not a verified stable release.

## What was missing
The previous bridge sent packet types 1 (clock), 2 (title), and 3 (media state). It never read or sent Windows output volume. The firmware also had no receiver for volume packets. UI-only changes could not fix that.

## Changes
- New type-4 packet: [4, sequence, percent, muted]. Percent 0..100; 255 means unavailable. Muted is 0 or 1.
- Reads the Windows default audio output endpoint using Pycaw, approximately every 0.5 seconds when other operations are not blocking.
- Actual fresh volume shown as VOL / 68%; manual estimates remain EST / ~68%.
- Drops actual-volume display after 4 seconds without fresh volume data.
- Resets actual volume on Bluetooth link changes.
- Preserves the uploaded carousel, centred dots, solid speaker icons and Media spacing.
- Adds dependency/audio/device diagnostics and traceback logging.
- Keeps the existing encrypted GATT service, HID keyboard controls, UUIDs and pins.

## Firmware install
1. Back up your current project.
2. Extract this ZIP into a new folder.
3. Open firmware/RotaryDeck_v2_1/RotaryDeck_v2_1.ino.
4. Keep the existing XIAO ESP32-S3 board/library settings (Arduino-ESP32 2.0.17 and T-vK BLE Keyboard 0.3.0).
5. Verify, then upload. Both headers must be the supplied canonical DeckCore.h and DisplayUI.h, not the old numbered upload filenames.

The .ino name remains V2.1 for compatibility with the existing project; the UI remains V2.2.

## Companion install (Windows, normal user)
1. Stop any older companion instance.
2. In companion/, double-click setup_companion.bat. It creates a local .venv and installs the supplied requirements. Python must already be available via the Windows py launcher. Never run as administrator just to fix setup errors.
3. Basic diagnostics should show dependencies, default audio volume and paired BLE device names. This does not prove a GATT connection.
4. Double-click start_companion.bat and leave the window open.
5. The runtime log should show the paired deck, sync-service discovery, Sync active and actual volume values.
6. Change Windows volume and check the LCD follows; test mute, time and media title.

If it fails, send the full terminal error or companion/rotary_deck_sync.log (review it first: it can contain device IDs and media titles). The window stays open. Do not remove/re-pair the deck unless the actual error points to stale service discovery/pairing.

If more than one paired deck is found, run in a terminal:
  start_companion.bat --list
Then choose its exact Windows device ID:
  start_companion.bat --device-id "YOUR DEVICE ID"

## Limitations / validation
- The volume is the default Windows output endpoint, not an individual application's volume. Switching the default output is re-read each poll.
- Metadata queries and BLE timeouts can delay volume updates; stale actual values are hidden.
- The prior connection failure is not diagnosed without its runtime error. Better diagnostics are provided, not a guaranteed Bluetooth repair.
- Python syntax and pure packet construction can be checked here; Windows COM/WinRT, Bluetooth, a real ESP32 build and LCD rendering cannot.
- No GitHub commit or release has been made.
