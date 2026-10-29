# Read-only filesystem image example

[LinuxBadFS.ds](LinuxBadFS.ds) presents the
included `badfs.img` as read-only USB mass storage.
Its host-side result depends on the Linux kernel, filesystem handling, and
automount configuration. This repository does not guarantee that connecting
the image will panic or reboot a host.

Treat the image as potentially disruptive. Use only in a disposable Linux
virtual machine with USB pass-through and a snapshot. Do not connect it to a
workstation, shared computer, or production system. The payload does not
install an operating system or add software persistence to the host.
