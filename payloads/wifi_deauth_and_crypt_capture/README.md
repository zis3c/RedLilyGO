# Wi-Fi scan and capture example

This DuckyScript example demonstrates a Marauder scan followed by a PMKID
capture command. It is not a passive-only demo. The script selects all access
points returned by its scan, so it has no per-target confirmation step and may
affect nearby devices.

## Review before use

- Use only inside an isolated, authorized RF lab. Do not run it on a shared,
  public, or production network.
- Read
  [PMKIDCapture.ds](PMKIDCapture.ds)
  and check the flags accepted by the Marauder version in
  this build before running it. The meaning of capture flags can vary by
  library revision.
- PCAP and related output may contain private traffic. Store it securely and
  remove it after the assessment.

## Run in a lab

1. Use a test board and test access point in the isolated lab.
2. Copy the linked `PMKIDCapture.ds` to `/Scripts/`.
3. Start the payload from the device's payload controls.
4. Use the payload's long-press stop action or BOOT triple-click to stop
   active work.
5. Confirm the capture has stopped and inspect the Files page before removing
   the SD card.

The scan and capture commands are queued by firmware. A queued response does
not prove that packets were captured or that a PCAP file was saved; check the
device log and storage listing.
