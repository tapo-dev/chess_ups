FROM debian:13

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build tools
    build-essential \
    make \
    gdb \
    \
    # General dev utilities
    git \
    ca-certificates \
    netcat-openbsd \
    less \
    nano \
    bash-completion \
    sudo \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Non-root user suitable for VS Code/devcontainers
ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=${USER_UID}

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd \
        --uid ${USER_UID} \
        --gid ${USER_GID} \
        --create-home \
        --shell /bin/bash \
        ${USERNAME} \
    && echo "${USERNAME} ALL=(root) NOPASSWD:ALL" \
        > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}

WORKDIR /workspaces/

CMD ["/bin/bash"]
