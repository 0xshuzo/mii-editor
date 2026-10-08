# Wii development container

This project includes an SSH-enabled Docker development environment for Wii
homebrew. It uses devkitPro's official `devkitpro/devkitppc` image, which
includes the `wii-dev` toolchain (devkitPPC, libogc, and Wii portlibs).

The container is deliberately reachable only from this computer:

- SSH is published as `127.0.0.1:2222` by default.
- The only login is `developer`.
- Password, keyboard-interactive, root, agent-forwarding, and X11 logins are
  disabled. A dedicated key is generated locally at `.wii-ssh/id_ed25519`.
- Build sources are the project root mounted at `/workspace`; compiler cache
  and SSH host keys live in Docker named volumes.

## Start it

From the project root:

```bash
./scripts/start-wii-container.sh
```

The first run creates the local SSH key and pulls/builds the image. It does
not place a key in your normal `~/.ssh` directory. To use another port:

```bash
WII_SSH_PORT=2223 ./scripts/start-wii-container.sh
```

To pin the devkitPro base image instead of following its `latest` tag:

```bash
DEVKITPPC_IMAGE=devkitpro/devkitppc:<tag> ./scripts/start-wii-container.sh
```

## Connect VSCodium

VSCodium cannot use Microsoft's proprietary **Remote - SSH** extension. Use
the Open VSX extension **Open Remote - SSH** by `jeanp413`
(`jeanp413.open-remote-ssh`) instead. After installing it, reload the VSCodium
window, then run **Remote - SSH: Connect to Host...** from the Command Palette.

Add this host entry to your SSH config if the start script did not already
print it for you; replace the path with the absolute path printed by the start
script.

```sshconfig
Host mii-editor-wii
    HostName 127.0.0.1
    Port 2222
    User developer
    IdentityFile /absolute/path/to/mii-editor/.wii-ssh/id_ed25519
    IdentitiesOnly yes
```

Use **Remote - SSH: Connect to Host...** → `mii-editor-wii`, then open `/workspace`.
The normal terminal is already configured with `DEVKITPRO`, `DEVKITPPC`, and
`PATH` for devkitPPC.

## Build the Mii Editor UI

Configure and build the Homebrew Channel app from the project root:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/wii-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The deployable folder is `build/apps/mii-editor/`. Copy it to the `apps/`
directory of a FAT32-formatted Wii SD card before launching it in the Homebrew Channel.

## Verify toolchain and connection

After the container starts, either run this from the host:

```bash
ssh -i .wii-ssh/id_ed25519 -p 2222 developer@127.0.0.1 \
  'echo "$DEVKITPPC"; powerpc-eabi-gcc --version'
```

or run the same commands from a VSCodium terminal. The expected toolchain
directory is `/opt/devkitpro/devkitPPC`.

## Stop or refresh

```bash
docker compose -f compose.wii.yml stop
docker compose -f compose.wii.yml down
docker compose -f compose.wii.yml up --build --detach
```

`down` leaves the named ccache and SSH-host-key volumes intact. To intentionally
discard them too, run `docker compose -f compose.wii.yml down --volumes`.

The devkitPro base image changes when it publishes a new `latest`. Rebuild to
get it, or pin `DEVKITPPC_IMAGE` for a release that needs a stable toolchain.
