# Start from a minimal Ubuntu base image.
FROM ubuntu:24.04

# Avoid interactive prompts during apt installs.
ENV DEBIAN_FRONTEND=noninteractive

# Install Ubuntu packages required by Zephyr's getting started flow.
RUN apt-get update && apt-get install -y \
    git \
    python3 \
    python3-pip \
    python3-venv \
    cmake \
    ninja-build \
    gperf \
    device-tree-compiler \
    wget \
    xz-utils \
    file \
    make \
    gcc \
    g++ \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create a Python virtual environment for all Zephyr tooling.
RUN python3 -m venv /opt/venv

# Use the virtual environment by default.
ENV PATH="/opt/venv/bin:${PATH}"

# Install west, Zephyr's workspace and meta-tool.
RUN pip install west

RUN wget -qO- https://code-server.dev/install.sh | sh

RUN code-server --install-extension nordic-semiconductor.nrf-connect \
    && code-server --install-extension nordic-semiconductor.nrf-devicetree \
    && code-server --install-extension nordic-semiconductor.nrf-kconfig \
    && code-server --install-extension nordic-semiconductor.nrf-terminal \
    && code-server --install-extension llvm-vs-code-extensions.vscode-clangd \
    && code-server --install-extension ms-vscode.cmake-tools

RUN mkdir -p /root/.local/share/code-server/User \
    && printf '%s\n' \
    '{' \
    '  "workbench.colorTheme": "Default Dark Modern",' \
    '  "editor.formatOnSave": true,' \
    '  "[c]": {' \
    '    "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"' \
    '  },' \
    '  "[cpp]": {' \
    '    "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"' \
    '  }' \
    '}' \
    > /root/.local/share/code-server/User/settings.json

# Copy only the project manifest into the image.
COPY west.yml /tmp/ws/manifest/west.yml

# Create a temporary west workspace using this project's manifest.
WORKDIR /tmp/ws
RUN west init -l manifest

# Download Zephyr and modules from the project manifest.
RUN west update

# Tell west where Zephyr is in this manifest layout.
RUN west config zephyr.base deps/zephyr

# Install Python packages for this full west workspace.
RUN west packages pip --install

# Install only the ARM Zephyr SDK toolchain into the image.
RUN west sdk install -t arm-zephyr-eabi

# Remove the temporary source workspace, but keep installed Python packages and SDK/toolchain.
WORKDIR /
RUN rm -rf /tmp/ws

COPY .devcontainer/entrypoint.sh /usr/local/bin/entrypoint.sh

# Strip out CRLF characters (Windows sometimes adds them) and make executable
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

# Default runtime workspace location.
WORKDIR /workspace

EXPOSE 8080

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

CMD ["bash"]
