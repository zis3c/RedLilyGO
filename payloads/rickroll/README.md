# Example: Rickroll

These DuckyScript examples send keyboard input to open a video URL on the
connected host. They do not start Marauder or display images. Host behavior
depends on USB HID mode, keyboard layout, focus, and host policy. Run only on a
host you own or are authorized to test.

## Setup

`RickrollKeys.ds` opens the video URL using Win+R. Copy the script to
`/Scripts/`, then run it from the payload list. Use USB Serial + HID for
keyboard input. Marauder remains available from the MRD page and is not started
by this script.
