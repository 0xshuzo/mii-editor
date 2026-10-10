# syntax=docker/dockerfile:1
# devkitPro publish this image with devkitPPC, libogc, and the Wii development
# packages already installed. Override DEVKITPPC_IMAGE at build time to pin a
# known-good image tag when a project needs a fully reproducible toolchain.
ARG DEVKITPPC_IMAGE=devkitpro/devkitppc:latest
FROM ${DEVKITPPC_IMAGE}

USER root

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        ca-certificates \
        cmake \
        ccache \
        curl \
        file \
        git \
        less \
        make \
        openssh-server \
        python3 \
        unzip \
        zip \
    && rm -rf /var/lib/apt/lists/*

# UID/GID 1000 matches the normal first Linux desktop user, so files made by
# VSCodium or make remain editable on the host bind mount.
ARG DEV_UID=1000
ARG DEV_GID=1000
RUN groupadd --gid "${DEV_GID}" developer \
    && useradd --uid "${DEV_UID}" --gid "${DEV_GID}" --create-home --shell /bin/bash developer \
    && passwd -d developer \
    && install -d -o developer -g developer -m 0700 /home/developer/.ssh \
    && install -d -o developer -g developer -m 0755 /workspace /home/developer/.cache /home/developer/.cache/ccache \
    && install -d -m 0755 /run/sshd /run/wii-ssh \
    && install -d -m 0700 /var/lib/wii-sshd

COPY sshd_config /etc/ssh/sshd_config.d/99-wii-development.conf
COPY entrypoint.sh /usr/local/bin/wii-ssh-entrypoint
RUN chmod 0755 /usr/local/bin/wii-ssh-entrypoint

ENV DEVKITPRO=/opt/devkitpro \
    DEVKITPPC=/opt/devkitpro/devkitPPC \
    CCACHE_DIR=/home/developer/.cache/ccache \
    CCACHE_COMPILERCHECK=content
ENV PATH="${DEVKITPPC}/bin:${PATH}"

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/wii-ssh-entrypoint"]
CMD ["/usr/sbin/sshd", "-D", "-e"]
