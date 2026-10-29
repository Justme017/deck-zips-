from pathlib import Path
import ast
import datetime

HELPER = 'async def _find_paired_devices(DeviceInformation, selector):\n    # Modern PyWinRT exposes Windows overloads under distinct names.\n    method = getattr(DeviceInformation, "find_all_async_aqs_filter", None)\n    if method is not None:\n        return await asyncio.wait_for(method(selector), 20)\n    method = getattr(DeviceInformation, "find_all_async_aqs_filter_and_additional_properties", None)\n    if method is not None:\n        return await asyncio.wait_for(method(selector, []), 20)\n    # Compatibility with older projections; retain the paired-device filter.\n    method = getattr(DeviceInformation, "find_all_async", None)\n    if method is not None:\n        for params in ((selector, []), (selector,)):\n            try:\n                operation = method(*params)\n            except TypeError:\n                continue\n            return await asyncio.wait_for(operation, 20)\n    available = [name for name in dir(DeviceInformation) if "find_all" in name]\n    raise RuntimeError(f"No compatible filtered enumeration overload. Available methods: {available}")\n\n\n'

def main():
    target = Path(__file__).resolve().with_name("rotary_deck_sync.py")
    source = target.read_text(encoding="utf-8-sig")
    old = "devices = await asyncio.wait_for(DeviceInformation.find_all_async(selector), 20)"
    new = "devices = await _find_paired_devices(DeviceInformation, selector)"
    if new in source and "async def _find_paired_devices(" in source:
        print("Enumeration fix already applied. No changes made.")
        return
    if source.count(old) != 1 or source.count("async def enumerate_devices(args):") != 1:
        raise RuntimeError("Unexpected companion version. No changes made. Send the current rotary_deck_sync.py.")
    updated = source.replace(old, new, 1).replace("async def enumerate_devices(args):", HELPER + "async def enumerate_devices(args):", 1)
    ast.parse(updated)
    backup = target.with_name(target.name + ".before-enumeration-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f") + ".bak")
    backup.write_bytes(target.read_bytes())
    temp = target.with_name(target.name + ".patching")
    temp.write_text(updated, encoding="utf-8")
    temp.replace(target)
    print("Enumeration fix applied. Original backed up as:", backup.name)
    print("Run setup_companion.bat again, then start_companion.bat.")

if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        traceback.print_exc()
        raise SystemExit(1)
