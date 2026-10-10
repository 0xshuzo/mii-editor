#!/usr/bin/env bash
set -euo pipefail

host_key_dir=/var/lib/wii-sshd
mounted_authorized_keys=/run/wii-ssh/authorized_keys
authorized_keys=/home/developer/.ssh/authorized_keys

# Host keys live on a named Docker volume, avoiding changed-host-key warnings
# every time the image is rebuilt or the container is recreated.
if [[ ! -s "${host_key_dir}/ssh_host_ed25519_key" ]]; then
    ssh-keygen -q -t ed25519 -f "${host_key_dir}/ssh_host_ed25519_key" -N ''
fi

if [[ ! -s "${host_key_dir}/ssh_host_rsa_key" ]]; then
    ssh-keygen -q -t rsa -b 4096 -f "${host_key_dir}/ssh_host_rsa_key" -N ''
fi

if [[ ! -s "${mounted_authorized_keys}" ]]; then
    echo "No SSH public key is mounted at ${mounted_authorized_keys}." >&2
    echo "Run ./scripts/start-wii-container.sh from the project root." >&2
    exit 1
fi

# Docker Desktop bind mounts do not preserve Unix ownership or modes. Copy the
# mounted public key into the image so sshd can verify its secure permissions.
install -o developer -g developer -m 0600 "${mounted_authorized_keys}" "${authorized_keys}"
chmod 0700 /home/developer/.ssh
chown -R developer:developer /home/developer/.cache/ccache 2>/dev/null || true

exec "$@"
