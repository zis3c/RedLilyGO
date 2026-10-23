# Example: Controlled storage lifecycle

Demonstrates switching between a first USB mass-storage presentation and a later read-only presentation. Use only with your own test media and host. It is not permission to hide another person's data.

## Setup

1. Back up the SD card and use only test files with no secrets.
2. Copy [StorageFlip.ds](StorageFlip.ds)
   to `/Scripts/` and `disk.img` to the SD card root.
   Extract `disk.img` from the included `disk.zip` archive first.
3. Boot the board and let the host finish mounting before disconnecting it.

Start `StorageFlip.ds` from the payload list. To run it at boot instead, copy
it to the SD card root as `/autorun.ds` and enable autorun.

On the first run, the script creates a `/firstrun` marker and selects USB mass
storage mode. On later runs, it mounts `/disk.img` read-only instead. This
changes which media the host sees; it does not encrypt, erase, or protect files.
The example also turns off Wi-Fi, display, and LED before choosing the storage
mode.

Always eject the host volume before unplugging. If stale contents remain, eject, wait for USB re-enumeration, and reconnect.
