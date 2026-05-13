# Upstream Sync Notes

This fork is based on:

```text
MisterTea/EternalTerminal@77eab5ec586e26106492f8403b18a35be0cb5de7
```

The clipboard image paste patch was added in:

```text
87ea062555ae1a9d97becdfdcfd8a4cb98a7feee Add clipboard image paste support
```

As of 2026-05-13, this fork is one commit ahead of upstream `master`.

## What This Fork Changes

The fork keeps the upstream EternalTerminal architecture and adds one feature:
when a macOS user presses `Ctrl+V` inside patched `et`, an image from the local
clipboard can be saved on the remote host and inserted into the remote terminal
as a file path.

Main behavior changes:

- `etterminal` prints `ETCAPS:clipboard-image-paste` during SSH setup.
- `et` detects that capability before enabling image paste frames.
- macOS `et` reads image bytes from `NSPasteboard` through JXA/`osascript`.
- macOS `et` also recognizes the local paste trigger
  `ESC ] 777 ; et-paste BEL`, intended for terminal keybindings such as Ghostty
  `Cmd+V`.
- When that trigger is received, images are sent as image frames and text
  clipboard content is pasted as text.
- The image is sent through the existing encrypted `TERMINAL_BUFFER` path.
- Remote `etterminal` decodes the private frame and writes the image to
  `/tmp/et-clipboard-images-<uid>/`.
- The remote PTY receives only the saved image path plus a trailing space.
- Old remotes are protected by the capability check, so binary frame data is not
  sent unless the remote says it supports the feature.

Files added:

```text
docs/clipboard-image-paste.md
docs/upstream-sync.md
src/terminal/ClipboardImageFrame.cpp
src/terminal/ClipboardImageFrame.hpp
src/terminal/LocalClipboardImage.cpp
src/terminal/LocalClipboardImage.hpp
test/unit_tests/ClipboardImageFrameTest.cpp
```

Upstream files modified:

```text
.gitignore
CMakeLists.txt
README.md
src/terminal/SshSetupHandler.cpp
src/terminal/SshSetupHandler.hpp
src/terminal/TerminalClient.cpp
src/terminal/TerminalClient.hpp
src/terminal/TerminalClientMain.cpp
src/terminal/TerminalMain.cpp
src/terminal/UserTerminalHandler.cpp
test/unit_tests/SshSetupHandlerTest.cpp
```

## How To Check Drift From Original

Fetch upstream:

```sh
git fetch origin master
```

Show this fork's patch compared with upstream:

```sh
git diff --stat origin/master..HEAD
git diff --name-status origin/master..HEAD
```

Show only fork commits:

```sh
git log --oneline origin/master..HEAD
```

If upstream has moved and you want to see what changed upstream since this fork's
base:

```sh
git log --oneline 77eab5ec586e26106492f8403b18a35be0cb5de7..origin/master
```

## Updating When Original Changes

Keep `origin` pointing at the original project:

```sh
git remote -v
```

Expected shape:

```text
origin   https://github.com/MisterTea/EternalTerminal.git
public   https://github.com/lambogreny/EternalTerminal-image-paste.git
```

If the public fork remote has a different name, replace `public` in the commands
below with that remote name.

Use a branch for the update:

```sh
git fetch origin master
git switch master
git switch -c sync-upstream-YYYYMMDD
git rebase origin/master
```

If there are conflicts, the likely files are:

```text
CMakeLists.txt
src/terminal/TerminalClient.cpp
src/terminal/SshSetupHandler.cpp
src/terminal/TerminalMain.cpp
src/terminal/UserTerminalHandler.cpp
test/unit_tests/SshSetupHandlerTest.cpp
```

Conflict resolution rule:

- Keep upstream fixes unless they directly remove the image-paste behavior.
- Keep `ClipboardImageFrame.*` and `LocalClipboardImage.*`.
- Keep `ETCAPS:clipboard-image-paste` output in `etterminal`.
- Keep the capability gate in `SshSetupHandler`.
- Keep `Ctrl+V` interception in `TerminalClient`.
- Keep the local paste trigger handling in `TerminalClient` and
  `LocalClipboardImage.*`.
- Keep remote temp-file saving in `UserTerminalHandler`.

After resolving conflicts:

```sh
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDISABLE_TELEMETRY=ON \
  -DBUILD_TESTING=ON

cmake --build build-release --target et etterminal etserver et-test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
ctest --test-dir build-release --output-on-failure
```

Push the sync branch:

```sh
git push public sync-upstream-YYYYMMDD
```

Merge it after testing real image paste through an ET session.

## Minimal Manual Test

1. Build patched macOS `et`.
2. Build patched Linux `etserver` and `etterminal` for the remote.
3. Start the remote `etserver` on a non-system port such as `20222`.
4. Connect with the patched local `et` and `--terminal-path` pointing to the
   patched remote `etterminal`.
5. Copy an image on macOS.
6. Press `Ctrl+V` inside the ET session.
7. Confirm the prompt receives a remote path like:

```text
/tmp/et-clipboard-images-1000/image-AbCd12.png
```

8. On the remote, verify:

```sh
file /tmp/et-clipboard-images-$(id -u)/image-*.png
```
