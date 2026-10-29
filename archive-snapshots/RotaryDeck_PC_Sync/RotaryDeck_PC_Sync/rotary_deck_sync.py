"""Rotary Deck V2.1 PC time/media bridge. Run on Windows as a normal user.
Uses paired-device enumeration (not scanning) so existing Windows HID connection
can share the device's custom GATT service. No shell commands or app launching.
"""
import argparse
import asyncio
import datetime as dt
import logging
import struct
import sys
import time
import unicodedata
import uuid

SERVICE_UUID = uuid.UUID("917e0001-d588-4b6b-a9a0-a81a964ce2b1")
RX_UUID = uuid.UUID("917e0002-d588-4b6b-a9a0-a81a964ce2b1")


def time_packet(sequence, timestamp=None, offset_minutes=None):
    if timestamp is None:
        timestamp = int(time.time())
    if offset_minutes is None:
        offset_minutes = int(dt.datetime.now().astimezone().utcoffset().total_seconds() / 60)
    return struct.pack("<BBIh", 1, sequence & 255, timestamp, offset_minutes)


def title_packets(sequence, title):
    text = unicodedata.normalize("NFKD", title or "")
    text = "".join(ch for ch in text if not unicodedata.combining(ch))
    text = " ".join(text.split()).encode("ascii", "replace")[:128]
    count = max(1, (len(text) + 15) // 16)
    return [bytes((2, sequence & 255, i, count)) + text[i * 16:(i + 1) * 16] for i in range(count)]


def state_packet(sequence, playback, session):
    return bytes((3, sequence & 255, playback, session))


async def enumerate_devices(args):
    from winrt.windows.devices.bluetooth import BluetoothLEDevice
    from winrt.windows.devices.enumeration import DeviceInformation
    selector = BluetoothLEDevice.get_device_selector_from_pairing_state(True)
    devices = await asyncio.wait_for(DeviceInformation.find_all_async(selector), 20)
    if args.list:
        for device in devices:
            print(f"{device.name}\n  {device.id}")
        return None
    matches = [d for d in devices if (d.id == args.device_id if args.device_id else d.name.casefold() == args.device_name.casefold())]
    if not matches:
        raise RuntimeError("No matching paired deck. Pair Rotary Deck in Windows Bluetooth settings first. Use --list to inspect names.")
    if len(matches) > 1:
        raise RuntimeError("More than one matching device. Use --list, then --device-id to choose explicitly.")
    return matches[0]


async def open_receiver(info):
    from winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
    from winrt.windows.devices.bluetooth.genericattributeprofile import GattCommunicationStatus
    device = None
    service = None
    try:
        device = await asyncio.wait_for(BluetoothLEDevice.from_id_async(info.id), 20)
        if device is None:
            raise RuntimeError("Windows could not open the paired deck. Check Bluetooth and device-access permissions.")
        result = await asyncio.wait_for(device.get_gatt_services_for_uuid_async(SERVICE_UUID, BluetoothCacheMode.UNCACHED), 20)
        if result.status != GattCommunicationStatus.SUCCESS or len(result.services) != 1:
            raise RuntimeError(f"Sync service unavailable ({result.status}). Flash V2.1; if upgrading from V2.0, remove/re-pair the deck to clear stale GATT cache.")
        service = result.services[0]
        chars = await asyncio.wait_for(service.get_characteristics_for_uuid_async(RX_UUID, BluetoothCacheMode.UNCACHED), 20)
        if chars.status != GattCommunicationStatus.SUCCESS or len(chars.characteristics) != 1:
            raise RuntimeError(f"Sync characteristic unavailable ({chars.status}).")
        return device, service, chars.characteristics[0]
    except Exception:
        if service is not None:
            service.close()
        if device is not None:
            device.close()
        raise


async def write_packet(characteristic, payload):
    from winrt.windows.storage.streams import DataWriter
    from winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption, GattCommunicationStatus
    if len(payload) > 20:
        raise ValueError("Protocol packets must fit the default 20-byte ATT payload.")
    writer = DataWriter()
    writer.write_bytes(payload)
    buffer = writer.detach_buffer()
    writer.close()
    result = await asyncio.wait_for(characteristic.write_value_with_result_async(buffer, GattWriteOption.WRITE_WITH_RESPONSE), 10)
    if result.status != GattCommunicationStatus.SUCCESS:
        raise RuntimeError(f"GATT write failed: {result.status}; check pairing/encryption and proximity.")
    await asyncio.sleep(0.025)


async def media_snapshot(manager):
    from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionPlaybackStatus as Status
    session = manager.get_current_session()
    if session is None:
        return 0, 0, ""
    props = await asyncio.wait_for(session.try_get_media_properties_async(), 5)
    status = session.get_playback_info().playback_status
    playback = 1 if status == Status.PLAYING else 2 if status == Status.PAUSED else 3 if status == Status.STOPPED else 0
    return playback, 1, props.title or ""


async def stream(characteristic):
    from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager
    manager = None
    last_time = -100.0
    sequence = 0
    last_title = None
    last_media_error = 0.0
    logging.info("Sync active: clock every 5s; media/heartbeat every 2s. Ctrl+C to exit.")
    while True:
        tick = time.monotonic()
        if tick - last_time >= 5:
            await write_packet(characteristic, time_packet(sequence))
            last_time = tick
        try:
            if manager is None:
                manager = await asyncio.wait_for(GlobalSystemMediaTransportControlsSessionManager.request_async(), 10)
            playback, session, title = await media_snapshot(manager)
        except Exception as error:
            playback, session, title = 0, 2, ""
            manager = None
            if tick - last_media_error >= 30:
                logging.warning("Media metadata unavailable (clock sync continues): %s", error)
                last_media_error = tick
        await write_packet(characteristic, state_packet(sequence, playback, session))
        for packet in title_packets(sequence, title):
            await write_packet(characteristic, packet)
        if title != last_title:
            logging.info("Media: %s", title or ("no session" if session == 0 else "title unavailable"))
            last_title = title
        sequence = (sequence + 1) & 255
        await asyncio.sleep(max(0.1, 2 - (time.monotonic() - tick)))


async def run(args):
    if args.list:
        await enumerate_devices(args)
        return
    while True:
        device = service = None
        try:
            info = await enumerate_devices(args)
            logging.info("Opening paired device: %s", info.name)
            device, service, characteristic = await open_receiver(info)
            await stream(characteristic)
        except asyncio.CancelledError:
            raise
        except Exception as error:
            logging.error("%s; retrying in 5 seconds", error)
        finally:
            if service is not None:
                service.close()
            if device is not None:
                device.close()
        await asyncio.sleep(5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device-name", default="Rotary Deck")
    parser.add_argument("--device-id", default=None)
    parser.add_argument("--list", action="store_true", help="List paired BLE devices and their Windows IDs.")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("This companion uses Windows Runtime APIs and runs on Windows only.")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        print("\nCompanion stopped. HID controls remain independent.")


if __name__ == "__main__":
    main()
