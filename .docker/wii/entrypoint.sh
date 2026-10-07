#!/usr/bin/env bash
set -euo pipefail

host_key_dir=/var/lib/wii-sshd
authorized_keys=/home/developer/.ssh/authorized_keys

# Host keys live on a named Docker volume, avoiding a changed-host-key warning
# every time the image is rebuilt or the container is recreated.
if [[ ! -s "${host_key_dir}/ssh_host_ed25519_key" ]]; then
    ssh-keygen -q -t ed25519 -f "${host_key_dir}/ssh_host_ed25519_key" -N ''
fi

if [[ ! -s "${host_key_dir}/ssh_host_rsa_key" ]]; then
    ssh-keygen -q -t rsa -b 4096 -f "${host_key_dir}/ssh_host_rsa_key" -N ''
fi

if [[ ! -s "${authorized_keys}" ]]; then
    echo "No SSH public key is mounted at ${authorized_keys}." >&2
    echo "Run ./scripts/start-wii-container.sh from the project root." >&2
    exit 1
fi

# sshd rejects a writable authorized_keys file. The mount is read-only in
# Compose, but the mode check remains useful if the service is run manually.
chmod 0700 /home/developer/.ssh
chmod 0600 "${authorized_keys}" 2>/dev/null || true
chown -R developer:developer /home/developer/.ssh 2>/dev/null || true
chown -R developer:developer /home/developer/.cache/ccache 2>/dev/null || true

exec "$@"
