#!/usr/bin/env bash
# Create a local, dedicated SSH identity and start the Wii build container.
# The key has no passphrase so VSCodium Remote-SSH can reconnect unattended.
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
key_directory="${project_root}/.wii-ssh"
private_key="${key_directory}/id_ed25519"
public_key="${private_key}.pub"
authorized_keys="${key_directory}/authorized_keys"

umask 077
mkdir -p "${key_directory}"

if [[ ! -f "${private_key}" ]]; then
    ssh-keygen -q -t ed25519 -f "${private_key}" -N '' -C "mii-editor Wii development container"
    echo "Created a dedicated local SSH key at ${private_key}."
fi

if [[ ! -f "${public_key}" ]]; then
    ssh-keygen -y -f "${private_key}" > "${public_key}"
fi

install -m 0600 "${public_key}" "${authorized_keys}"

export DEV_UID="$(id -u)"
export DEV_GID="$(id -g)"
export WII_SSH_PORT="${WII_SSH_PORT:-2222}"

cd "${project_root}"
docker compose -f compose.wii.yml up --build --detach

cat <<EOF

Wii build container is running. In VSCodium's SSH config, add:

Host mii-editor-wii
    HostName 127.0.0.1
    Port ${WII_SSH_PORT}
    User developer
    IdentityFile ${private_key}
    IdentitiesOnly yes

In VSCodium, use Open Remote - SSH's “Remote - SSH: Connect to Host...”
command and select mii-editor-wii. Open /workspace.
EOF
