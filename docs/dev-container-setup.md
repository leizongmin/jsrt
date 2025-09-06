# Dev Container and Docker Setup for jsrt

This document describes the development container and Docker setup for the jsrt project.

## 🐳 Dev Container (VS Code)

The project includes a VS Code Dev Container configuration that provides a complete development environment with all necessary dependencies.

### Features
- Ubuntu 22.04 base image
- Pre-installed development tools: gcc, cmake, git, clang-format-20
- VS Code extensions for C/C++ development
- Automatic submodule initialization and project build
- Consistent development environment across platforms

### Usage
1. Install [VS Code](https://code.visualstudio.com/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open the project in VS Code
3. When prompted, click "Reopen in Container" or use `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"
4. Wait for the container to build and initialize
5. Start developing!

### Configuration
The dev container configuration is in `.devcontainer/devcontainer.json`. It includes:
- Automatic package installation and setup
- VS Code extensions for C/C++ development
- Proper clang-format configuration
- Git integration

## 🔧 Docker Setup for Claude Code

The project provides Docker-based environment for running Claude Code with all project dependencies pre-installed.

### Available Commands

#### Build Docker Image
```bash
make docker-build
```
Builds the `jsrt-claude-dev:latest` Docker image with all dependencies.

#### Run Claude Code Environment
```bash
make claude
```
Starts Claude Code in a Docker container with:
- Current repository mapped to `/repo` inside container
- All development dependencies available
- Claude Code configured with unsafe operations allowed (when supported)

#### Interactive Shell
```bash
make claude-shell
```
Starts an interactive bash shell in the development environment for debugging.

#### Clean Docker Resources
```bash
make docker-clean
```
Removes the Docker image to save space.

### Docker Environment Details

The Docker environment (`Dockerfile.claude`) includes:
- **Base**: Ubuntu 22.04
- **Build tools**: gcc, cmake, make, git
- **Formatting**: clang-format-20
- **Development**: Node.js, VS Code, Python 3
- **Claude Code**: Placeholder installation (needs actual Claude Code)

### Updating Claude Code Installation

The Claude Code installation in the Docker image is currently a placeholder. To install the actual Claude Code:

1. Edit `Dockerfile.claude`
2. Replace the placeholder `claude-code` script with actual installation commands
3. Update the script to use the correct Claude Code binary and flags
4. Rebuild with `make docker-build`

Example update needed in `Dockerfile.claude`:
```dockerfile
# Replace this section with actual Claude Code installation
RUN echo '#!/bin/bash' > /usr/local/bin/claude-code \
    && echo 'exec /path/to/real/claude-code --allow-unsafe-operations "$@"' >> /usr/local/bin/claude-code \
    && chmod +x /usr/local/bin/claude-code
```

### Environment Variables

When Claude Code runs in the container, the following environment is set:
- `CLAUDE_ALLOW_UNSAFE_OPERATIONS=1` - Enables unsafe operations
- Working directory: `/repo` (mapped to project root)
- PATH includes all development tools

### Troubleshooting

#### Docker Build Fails
- Ensure Docker is installed and running
- Check internet connectivity for package downloads
- Try `make docker-clean` and `make docker-build` again

#### Container Cannot Access Repository
- Verify the repository path in the volume mount
- Check Docker permissions on the host system

#### Claude Code Not Found
- The current setup uses a placeholder
- Update `Dockerfile.claude` with actual Claude Code installation
- Rebuild the image after updates

## 📝 Integration with Existing Workflow

Both setups integrate seamlessly with the existing jsrt development workflow:

- All `make` commands work inside containers
- Git submodules are properly initialized
- Code formatting with `make clang-format` works
- Testing with `make test` is available
- The `.claude/` agent configuration is accessible

## 🚀 Quick Start

### For VS Code Users
```bash
# Open in VS Code Dev Container
code .
# VS Code will prompt to reopen in container
```

### For Claude Code Users
```bash
# Build and run Claude Code environment
make claude
```

### For Development
```bash
# Interactive development environment
make claude-shell

# Inside container:
make clang-format  # Format code
make test         # Run tests
make              # Build project
```