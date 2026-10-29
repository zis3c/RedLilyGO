# ruff: noqa: F821  (Import and env are injected by PlatformIO's SCons build)
"""Allow AsyncTCPSock's listening socket to rebind a recently used port.

AsyncServer::begin() binds without SO_REUSEADDR. The dashboard closes its
listener whenever the Evil Portal takes port 80, but lwIP keeps the old
listening PCB around, so the portal's bind fails with EADDRINUSE and the
portal AP comes up with no HTTP listener. SO_REUSEADDR lets the new bind
succeed (verified on hardware with a raw bind test).
"""

import os

Import("env")

ORIGINAL = """    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in server;
"""

PATCHED = """    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return;

    {
      const int reuse = 1;
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    struct sockaddr_in server;
"""

MARKER = "SO_REUSEADDR"


def patch():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    target = os.path.join(libdeps, env_name, "AsyncTCPSock", "src", "AsyncTCP.cpp")

    if not os.path.isfile(target):
        print("[patch] AsyncTCPSock AsyncTCP.cpp not found at %s; skipping" % target)
        return

    with open(target, "r", encoding="utf-8") as handle:
        source = handle.read()

    if MARKER in source:
        print("[patch] AsyncTCPSock listener already sets SO_REUSEADDR")
        return

    if ORIGINAL not in source:
        print("[patch] WARNING: expected AsyncTCPSock code not found; skipping patch")
        return

    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(source.replace(ORIGINAL, PATCHED, 1))
    print("[patch] AsyncTCPSock listener now sets SO_REUSEADDR")


patch()
