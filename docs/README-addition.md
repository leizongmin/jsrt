# Development Environment Setup

This section covers the new development environment options for jsrt.

## 🐳 VS Code Dev Container

Get started with a fully configured development environment in seconds:

1. Install [VS Code](https://code.visualstudio.com/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open the project in VS Code
3. Click "Reopen in Container" when prompted
4. Start coding! All dependencies are pre-installed and configured.

**Features:**
- Ubuntu 22.04 with all build tools
- Pre-configured C/C++ development environment
- Automatic project build and submodule initialization
- Consistent environment across all platforms

## 🤖 Claude Code Docker Environment

Run Claude Code in a fully configured Docker environment:

```bash
# Build the Docker image (one-time setup)
make docker-build

# Start Claude Code with the repository mapped to /repo
make claude

# Or start an interactive shell for manual development
make claude-shell
```

**Features:**
- All project dependencies pre-installed
- Repository automatically mapped to `/repo`
- Claude Code configured with unsafe operations allowed
- Git and build tools ready to use

## 📋 Available Commands

Get a list of all available make commands:

```bash
make help
```

Key commands for development environments:
- `make docker-build` - Build the Claude Code Docker image
- `make claude` - Run Claude Code in Docker
- `make claude-shell` - Interactive development shell
- `make docker-clean` - Clean up Docker resources

## 📖 Documentation

- [Complete Dev Container Setup Guide](docs/dev-container-setup.md)
- [Troubleshooting Guide](docs/troubleshooting-docker.md)

---

*Add this section to the main README.md under a "Development Environment" heading*