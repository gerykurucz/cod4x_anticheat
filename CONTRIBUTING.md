# Contributing to Nektum Shield

First of all, thank you for considering contributing to **Nektum Shield**!  
We welcome all kinds of contributions: bug reports, feature requests, code improvements, documentation, and testing.

## Code of Conduct

Please be respectful and constructive in all interactions.  
We follow a simple rule: **don't be a jerk**.

## How Can I Contribute?

### Reporting Bugs

- **Ensure the bug was not already reported** by searching the [Issues](https://github.com/sudoxv9k/cod4x_anticheat/issues).
- If you cannot find an existing issue, [open a new one](https://github.com/sudoxv9k/cod4x_anticheat/issues/new). Include:
  - A clear title and description.
  - Steps to reproduce.
  - Expected vs. actual behavior.
  - Server OS (Linux distribution or Windows version).
  - Plugin version (check console on start).
  - Any relevant log excerpts (`nektumshield.log`).

### Suggesting Features

Open an issue with the label `enhancement`. Describe the feature, its use case, and how it would benefit server admins.

### Pull Requests

1. **Fork** the repository and create a new branch from `main`.
2. **Make your changes** – keep them focused and well‑tested.
3. **Follow the coding style** used in the project (see below).
4. **Document** any new cvars, commands, or file formats.
5. **Test** your changes on a local COD4X server (if possible).
6. **Submit a pull request** with a clear description.
7. **Explain why, not what.**

## Development Setup

### Requirements

- **Linux** (any modern distribution) with 32‑bit GCC
- COD4X Plugin SDK – headers (`api/`)
- Basic knowledge of C and Make.

### Building

Follow [README.md](https://github.com/sudoxv9k/cod4x_anticheat/blob/main/README.md) building section depending on your linux distrobution.

## Testing

- Set up a local COD4X server.
- Place the compiled plugin in the plugins directory
- Load the plugin & setup your server config based on the included example in the repository.
- Load the plugin and verify it starts without errors.
- Test admin commands (`bb`, `ub`, `fu`, `mute`, `unmute`).
- Trigger detections manually if possible, or observe normal gameplay.
- Check that data files are created and Discord messages are sent (if webhook is set).

## Documentation

If your change adds a new cvar, command, or file format, please update the [README.md](https://github.com/sudoxv9k/cod4x_anticheat/blob/main/README.md) accordingly.

## Questions?

Feel free to open an issue with the `question` label or contact the maintainer directly.

---
## Security
Do not post secrets, API keys, private logs, personal documents, or public IPs in issues or pull requests.

For security reports, follow [SECURITY.md](https://github.com/sudoxv9k/cod4x_anticheat/blob/main/SECURITY.md).

## Thank you for helping make Nektum Shield better!