# RotaryDeck archive inventory (review PR only)

Scope: inspected 8 listed ZIP archives in this repository without modifying original ZIPs, executing archived scripts, or installing dependencies.

## Safety gates used before extraction

- Rejected unsafe paths: absolute paths and `..` traversal segments.
- Rejected symlinks in ZIP metadata.
- Prechecked archive limits before extraction: max 3,000 entries/archive; max 30 MiB uncompressed/archive; max 2 MiB/file.
- Extracted only text source/docs into `archive-snapshots/<archive-stem>/`, preserving archive-relative paths.
- Excluded `.venv`/`venv`, `site-packages`, caches (`__pycache__`, pytest/mypy caches), compiled/binary artifacts, logs, and credential-like filenames.
- For the large volume-sync candidate, extraction was intentionally selective (15 included paths, 1,371 excluded paths).

## Archive-by-archive inventory

| Archive | SHA-256 | Included paths | Excluded paths | Entry points (source) | Version labels seen in files | Dependencies status | Classification |
|---|---|---:|---:|---|---|---|---|
| `RotaryDeck_v1_complete.zip` | `0d80b45eb228a218d09c3e7180c9190486ce1ebe08c7078682d33121b647d772` | 5 | 0 | `RotaryDeck_v1/RotaryDeck_v1.ino`; `RotaryDeck_v1/DeckFirmware.h`; optional `WindowsApps.ahk` helper | `v1.0` in README | README lists target libs (Arduino-ESP32 2.0.17, T-vK BLE Keyboard 0.3.0, U8g2 2.x); no machine-readable manifest | Standalone firmware snapshot + optional Windows helper |
| `RotaryDeck_v1_2_modular.zip` | `5c11146d5e4c3ab436fd206a6ad1d9c00cce7b68f42de195dd226636e03b2282` | 3 | 0 | `RotaryDeck_v1_2/RotaryDeck_v1_2.ino` with `DeckCore.h`/`DisplayUI.h` | `V1.2`/`v1.2` | No dependency manifest included | Standalone firmware snapshot |
| `RotaryDeck_v2_0.zip` | `f2e6d4f9c031e9f6bb1ec1e13ecf1653aad936a1a0b06e4ce852533afbdb9d45` | 3 | 0 | `RotaryDeck_v2_0/RotaryDeck_v2_0/RotaryDeck_v2_0.ino` with headers | `V2.0` | No dependency manifest included | Standalone firmware snapshot |
| `RotaryDeck_v2_1_firmware.zip` | `4bd72d12734170a9d4112c2d3e70971a27f8842700745cc70820da68e2599926` | 4 | 0 | `RotaryDeck_v2_1_firmware/RotaryDeck_v2_1/RotaryDeck_v2_1.ino` with headers | `V2.1` | No dependency manifest included | Standalone firmware snapshot (contains one `.bak` header backup) |
| `RotaryDeck_PC_Sync.zip` | `c86cd49558f0e0dfcf030dfefba87229a7d5986399c977e8d7b065bac973e8b6` | 2 | 0 | `RotaryDeck_PC_Sync/rotary_deck_sync.py` | docstring references `V2.1`/`V2.0` compatibility context | `requirements.txt` present (WinRT packages); no firmware files | Companion-only archive |
| `RotaryDeck_v2_2_VolumeSync_Candidate.zip` | `fec391c2c66b67fd96e2a7e39f49415d9e741d8fedfe4ed33bb897394f7d98ed` | 15 | 1,371 | `.../firmware/RotaryDeck_v2_1/RotaryDeck_v2_1.ino`; `.../companion/rotary_deck_sync.py` | README labels `v2.2` candidate; firmware folder remains `v2_1` naming | `companion/requirements.txt` present (+`pycaw`); archive also includes vendored `.venv/site-packages` (excluded) | Composite candidate snapshot (firmware + companion), not treated as stable release |
| `RotaryDeck_v2_2_Complete_Dashboard_Date.zip` | `0bf552e1dba535fa687e15a3e7df82c7a49997c3a7a6964548111c4431441543` | 3 | 0 | `DeckCore.h` + `DisplayUI.h` replacement set; no `.ino` | INSTALL labels `v2.2` dashboard/date add-on over `RotaryDeck_v2_1` | Depends on existing working `RotaryDeck_v2_1` sketch and existing companion; no manifest | Incremental firmware header patch |
| `RotaryDeck_BLE_Keyboard_Reconnect_Candidate.zip` | `5dab176b43d5f31e21d0568aa8f59605ecc965ddc03273b7e4a815cb90919a08` | 3 | 0 | `BleKeyboard.cpp` (+ `BleKeyboard.h`) library replacement | no explicit semantic version tag in files; INSTALL targets BLE Keyboard 0.3.0 context | Depends on installed Arduino BLE Keyboard library; no full project manifest | Incremental library patch (companion-independent) |

## Companion enumeration and GATT overload fixes (what is actually present)

In `RotaryDeck_v2_2_VolumeSync_Candidate`:

- **Enumeration fix is present** in active `companion/rotary_deck_sync.py`:
  - includes `_find_paired_devices(...)` helper
  - checks `find_all_async_aqs_filter`, `find_all_async_aqs_filter_and_additional_properties`, then fallback `find_all_async`
- **GATT overload fix is present** in active `companion/rotary_deck_sync.py`:
  - `get_gatt_services_for_uuid_with_cache_mode_async(...)`
  - `get_characteristics_for_uuid_with_cache_mode_async(...)`
  - `write_value_with_result_and_option_async(...)`
- Pre-fix backups are also present (`.before-enumeration...bak`, `.before-gatt-overloads...bak`) and show prior call forms.

## Proposed composition for later human review (described only; not applied)

Not applied in this PR. Candidate composition to evaluate:

1. **Firmware base**: `RotaryDeck_v2_2_VolumeSync_Candidate/.../firmware/RotaryDeck_v2_1/` (includes volume-sync packet receiver path).
2. **Dashboard/date headers overlay**: `RotaryDeck_v2_2_Complete_Dashboard_Date.zip` `DeckCore.h` + `DisplayUI.h` (as documented incremental header replacement).
3. **Corrected companion**: `RotaryDeck_v2_2_VolumeSync_Candidate/.../companion/rotary_deck_sync.py` + its `requirements.txt` (already containing enumeration and GATT overload fixes).
4. **Reconnect-patched library**: `RotaryDeck_BLE_Keyboard_Reconnect_Candidate.zip` `BleKeyboard.cpp` (and matching header if needed).

Reconnect outcome note: the reconnect patch was reported by the user as working on their device; this is **user-reported**, not agent-verified.

## License notices

No explicit license files or license headers were found in inspected extracted paths. No new license was assigned.

## Blocked archives and uncertainties

- **Blocked archives:** none (all eight passed the defined safety prechecks).
- **Uncertainties for migration planning:**
  - Firmware archives do not include a machine-readable dependency lock/manifest.
  - Several packages are incremental overlays/patches rather than complete standalone projects.
  - Hardware behavior (BLE reconnect reliability, display behavior, wiring assumptions) is documented in upstream text but not verified in this review.
  - Archive-provided validation statements are narrative; no reproducible CI/test harness is included in these ZIPs.
