from pathlib import Path
import ast
import datetime

REPLACEMENTS = {'device.get_gatt_services_for_uuid_async(SERVICE_UUID, BluetoothCacheMode.UNCACHED)': 'device.get_gatt_services_for_uuid_with_cache_mode_async(SERVICE_UUID, BluetoothCacheMode.UNCACHED)', 'service.get_characteristics_for_uuid_async(RX_UUID, BluetoothCacheMode.UNCACHED)': 'service.get_characteristics_for_uuid_with_cache_mode_async(RX_UUID, BluetoothCacheMode.UNCACHED)', 'characteristic.write_value_with_result_async(buffer, GattWriteOption.WRITE_WITH_RESPONSE)': 'characteristic.write_value_with_result_and_option_async(buffer, GattWriteOption.WRITE_WITH_RESPONSE)'}

def patch_source(source):
    updated = source
    changes = 0
    for old, new in REPLACEMENTS.items():
        if updated.count(old) == 1 and new not in updated:
            updated = updated.replace(old, new, 1)
            changes += 1
        elif old not in updated and updated.count(new) == 1:
            pass
        else:
            raise RuntimeError("Unexpected companion code for: " + old + ". No changes saved. Send the current rotary_deck_sync.py.")
    ast.parse(updated)
    return updated, changes

def main():
    target = Path(__file__).resolve().with_name("rotary_deck_sync.py")
    source = target.read_text(encoding="utf-8-sig")
    updated, changes = patch_source(source)
    if changes == 0:
        print("All three GATT overload fixes are already applied. No changes made.")
        return
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup = target.with_name(target.name + ".before-gatt-overloads-" + stamp + ".bak")
    backup.write_bytes(target.read_bytes())
    temp = target.with_name(target.name + ".patching")
    temp.write_text(updated, encoding="utf-8")
    temp.replace(target)
    print("Applied", changes, "GATT overload fixes.")
    print("Backup:", backup.name)
    print("Your enumeration fix and volume-sync code are preserved.")
    print("Now run start_companion.bat. No firmware upload or dependency reinstall needed.")

if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        traceback.print_exc()
        raise SystemExit(1)
