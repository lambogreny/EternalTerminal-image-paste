# Clipboard Image Paste

This fork adds image paste support for EternalTerminal sessions used with
terminal AI tools such as Claude Code or Codex running on a remote host.

The feature is intentionally client/terminal scoped. It does not require a new
etserver protocol packet, and it does not require installing over the system
`et`, `ssh`, or `mosh` binaries.

## What Changed

- macOS `et` client watches for `Ctrl+V` while connected to a compatible remote
  `etterminal`.
- If the local macOS clipboard contains an image, the client reads PNG, JPEG,
  GIF, TIFF, or a convertible `NSImage` from the pasteboard.
- The image is encoded into an internal `TERMINAL_BUFFER` frame and sent through
  the existing encrypted EternalTerminal connection.
- Remote `etterminal` recognizes the internal frame, writes the image to:

  ```sh
  /tmp/et-clipboard-images-$(id -u)/image-XXXXXX.<ext>
  ```

- The remote path is inserted into the PTY as normal text with a trailing space.
  This means shells, tmux, Claude Code, and Codex see a readable local file path.
- `etterminal` advertises support with:

  ```text
  ETCAPS:clipboard-image-paste
  ```

  The macOS client only sends image frames after seeing that capability, so a new
  client talking to an old remote should fall back to ordinary `Ctrl+V` input.

## Current Scope

- Local image capture is implemented for macOS clients.
- Remote image save works on POSIX hosts. It is disabled on Windows.
- Maximum image size is 25 MiB.
- Image files are left in `/tmp/et-clipboard-images-<uid>/` until removed by the
  user or normal `/tmp` cleanup.
- Use `Ctrl+V` inside the ET session. `Cmd+V` is handled by the terminal app and
  is not visible to CLI programs unless your terminal emulator is configured to
  send ET's local clipboard paste trigger.

## คู่มือติดตั้งให้เครื่องอื่น

ภาพรวมสั้นๆ:

- ฝั่ง Mac คือเครื่องที่เรากด copy รูปและกด `Ctrl+V`
- ฝั่ง server คือเครื่องที่รัน `claude`, `codex`, `tmux`, shell
- Mac ต้องใช้ binary `et` ตัว patched
- Server ต้องใช้ binary `etserver` และ `etterminal` ตัว patched
- ไม่ต้องทับ `ssh`, `mosh`, หรือ `et` เดิมในระบบ ให้ copy binary ไว้ใต้
  `~/.local/et-image-paste/bin`

### 1. Build ฝั่ง Mac

Clone this repo:

```sh
git clone git@github.com:lambogreny/EternalTerminal-image-paste.git
cd EternalTerminal-image-paste
```

Build release:

```sh
cmake -S . -B build-macos-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDISABLE_TELEMETRY=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos-release --target et etterminal et-test -j"$(sysctl -n hw.ncpu)"
ctest --test-dir build-macos-release --output-on-failure
```

ถ้า Mac ยังไม่มี dependency สำหรับ build ให้ลงเฉพาะ dependency ได้ แต่อย่าลง
package `et` จาก package manager เพราะจะทำให้สับสนกับตัว patched:

```sh
brew install cmake ninja protobuf libsodium gflags pkg-config
```

ติดตั้งแบบ user-space:

```sh
mkdir -p ~/.local/et-image-paste/bin ~/.local/bin
cp build-macos-release/et build-macos-release/etterminal \
  ~/.local/et-image-paste/bin/
```

### 2. Build ฝั่ง Server

ถ้า server เป็น Linux x86_64 ให้ clone repo บน server แล้ว build binary Linux:

```sh
git clone git@github.com:lambogreny/EternalTerminal-image-paste.git
cd EternalTerminal-image-paste

cmake -S . -B build-linux-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDISABLE_TELEMETRY=ON \
  -DBUILD_TESTING=ON

cmake --build build-linux-release --target etserver etterminal et-test -j"$(nproc)"
ctest --test-dir build-linux-release --output-on-failure
```

ถ้า server ยังไม่มี dependency และยอมลง package สำหรับ build ได้ ตัวอย่างบน
Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  protobuf-compiler libprotobuf-dev \
  libsodium-dev libgflags-dev zlib1g-dev
```

ถ้าไม่อยากลง dependency ลงบน host ให้ build ใน Docker แทน แล้ว copy binary ออกมา:

```sh
docker run --rm -v "$PWD":/src -w /src ubuntu:24.04 bash -lc '
  apt-get update &&
  apt-get install -y build-essential cmake ninja-build pkg-config \
    protobuf-compiler libprotobuf-dev libsodium-dev libgflags-dev zlib1g-dev &&
  cmake -S . -B build-linux-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DDISABLE_TELEMETRY=ON \
    -DBUILD_TESTING=ON &&
  cmake --build build-linux-release --target etserver etterminal et-test -j"$(nproc)" &&
  ctest --test-dir build-linux-release --output-on-failure
'
```

ติดตั้งบน server แบบ user-space:

```sh
mkdir -p ~/.local/et-image-paste/bin ~/.local/et-image-paste/run
cp build-linux-release/etserver build-linux-release/etterminal \
  ~/.local/et-image-paste/bin/
```

ถ้า build Linux จากเครื่องอื่น ให้ใช้ `scp`:

```sh
ssh l1 'mkdir -p ~/.local/et-image-paste/bin ~/.local/et-image-paste/run'
scp build-linux-release/etserver build-linux-release/etterminal \
  l1:~/.local/et-image-paste/bin/
```

### 3. สร้าง Command สำหรับเข้า Server จาก Mac

ตัวอย่างนี้สร้างคำสั่ง `et-l1` โดยไม่แตะ command `et` เดิม:

```sh
cat > ~/.local/bin/et-l1 <<'EOF'
#!/bin/sh
set -eu

LOCAL_HOME="${ET_IMAGE_PASTE_HOME:-$HOME/.local/et-image-paste}"
LOCAL_ET="$LOCAL_HOME/bin/et"
REMOTE_HOST="${ET_IMAGE_PASTE_HOST:-l1}"
REMOTE_PORT="${ET_IMAGE_PASTE_PORT:-20222}"
REMOTE_HOME="${ET_IMAGE_PASTE_REMOTE_HOME:-/home/l1/.local/et-image-paste}"
REMOTE_BIN="$REMOTE_HOME/bin"
REMOTE_RUN="$REMOTE_HOME/run"
LOCAL_TERM="${TERM:-xterm-256color}"

if [ "$LOCAL_TERM" = "dumb" ]; then
  LOCAL_TERM="xterm-256color"
fi

ssh "$REMOTE_HOST" "
  set -eu
  mkdir -p '$REMOTE_RUN'
  if ! (ss -ltn 2>/dev/null | grep -q ':$REMOTE_PORT '); then
    oldpid=\$(cat '$REMOTE_RUN/etserver.pid' 2>/dev/null || true)
    if [ -n \"\$oldpid\" ]; then
      kill \"\$oldpid\" 2>/dev/null || true
      sleep 1
    fi
    '$REMOTE_BIN/etserver' --port '$REMOTE_PORT' --daemon \
      --pidfile '$REMOTE_RUN/etserver.pid' --logdir '$REMOTE_RUN'
    sleep 1
  fi
"

exec env TERM="$LOCAL_TERM" "$LOCAL_ET" \
  -p "$REMOTE_PORT" \
  --terminal-path "$REMOTE_BIN/etterminal" \
  "$@" "$REMOTE_HOST"
EOF

chmod +x ~/.local/bin/et-l1
```

ถ้า server ไม่ได้ชื่อ `l1` ให้แก้ `REMOTE_HOST` และ `REMOTE_HOME` ในไฟล์ wrapper
ให้ตรงกับ server นั้น เช่น:

```sh
REMOTE_HOST="${ET_IMAGE_PASTE_HOST:-myserver}"
REMOTE_HOME="${ET_IMAGE_PASTE_REMOTE_HOME:-/home/myuser/.local/et-image-paste}"
```

### 4. ใช้งานจริง

เข้า server ด้วย wrapper:

```sh
et-l1
```

บน server เปิด tmux หรือ tool ที่ต้องการ:

```sh
tmux attach -t test
claude
```

จากนั้นบน Mac:

1. Copy รูป
2. Focus terminal ที่กำลังรัน `et-l1`
3. กด `Ctrl+V`
4. จะเห็น path ประมาณนี้ถูกใส่ใน prompt:

```sh
/tmp/et-clipboard-images-1000/image-AbCd12.png
```

กด Enter ได้เลย เพราะไฟล์นี้อยู่บน server แล้ว ไม่ใช่ path บน Mac

### 5. เช็คว่าติดตั้งถูกไหม

เช็ค Mac:

```sh
command -v et-l1
ls -lh ~/.local/et-image-paste/bin/et
```

เช็ค server:

```sh
ssh l1 'ls -lh ~/.local/et-image-paste/bin/etserver ~/.local/et-image-paste/bin/etterminal'
ssh l1 '(ss -ltnp 2>/dev/null || true) | grep 20222'
```

ทดสอบ paste แล้วดูไฟล์บน server:

```sh
ssh l1 'ls -lh /tmp/et-clipboard-images-$(id -u)/'
```

ถ้าใช้ `ssh l1` หรือ `mosh l1` ตรงๆ จะส่งรูปไม่ได้ ต้องเข้าโดย `et-l1`
หรือ wrapper ของ patched ET เท่านั้น

### 6. ทำให้ `Cmd+V` ใช้ได้แบบ Native บน Ghostty

บน macOS โปรแกรม CLI มองไม่เห็นปุ่ม `Command` โดยตรง เพราะ terminal emulator
เป็นคนจับ `Cmd+V` ก่อน ถ้าอยากให้ `Cmd+V` เรียก image paste ของ ET ต้องให้
terminal ส่ง trigger พิเศษนี้เข้ามาแทน:

```text
ESC ] 777 ; et-paste BEL
```

ใน Ghostty ใส่บรรทัดนี้ใน config:

```text
keybind = cmd+v=text:\x1b]777;et-paste\x07
```

จากนั้น reload Ghostty config ด้วย `Cmd+Shift+,` หรือเปิด Ghostty ใหม่

เมื่อกด `Cmd+V` ใน patched ET:

- ถ้า clipboard เป็นรูป: ET จะ upload รูปไป server แล้วใส่ remote path
- ถ้า clipboard เป็นข้อความ: ET จะ paste ข้อความจาก macOS clipboard
- ถ้าอยากปิด trigger นี้ในบาง session:

```sh
ET_DISABLE_LOCAL_CLIPBOARD_PASTE_TRIGGER=1 et-l1
```

ข้อควรระวัง: keybind นี้เป็น config ของ Ghostty ถ้าเปิดใช้ global ใน Ghostty
แล้วกด `Cmd+V` ใน shell ธรรมดาที่ไม่ได้รัน patched ET อาจมี sequence แปลกๆ
หลุดเข้า shell ได้ แนะนำให้ใช้กับ profile/window ที่ไว้เข้า ET เป็นหลัก หรือ
ใช้ `Ctrl+V` ต่อไปถ้าไม่อยากเปลี่ยนพฤติกรรม paste ของ Ghostty ทั้งแอป

## Files Added Or Touched

- `src/terminal/ClipboardImageFrame.hpp`
- `src/terminal/ClipboardImageFrame.cpp`
- `src/terminal/LocalClipboardImage.hpp`
- `src/terminal/LocalClipboardImage.cpp`
- `src/terminal/TerminalClient.cpp`
- `src/terminal/TerminalClient.hpp`
- `src/terminal/UserTerminalHandler.cpp`
- `src/terminal/SshSetupHandler.cpp`
- `src/terminal/SshSetupHandler.hpp`
- `src/terminal/TerminalClientMain.cpp`
- `src/terminal/TerminalMain.cpp`
- `test/unit_tests/ClipboardImageFrameTest.cpp`
- `test/unit_tests/SshSetupHandlerTest.cpp`

## Build

Build the patched binaries instead of installing over system ET:

```sh
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDISABLE_TELEMETRY=ON \
  -DBUILD_TESTING=ON

cmake --build build-release --target et etterminal etserver et-test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
ctest --test-dir build-release --output-on-failure
```

If a machine does not already have the ET build dependencies, install or build
them in a user-controlled environment such as Homebrew, vcpkg, or Docker. Do not
run `cmake --install` unless you explicitly want to replace the machine-wide ET.

## User-Space Install Layout

Recommended local layout:

```sh
mkdir -p ~/.local/et-image-paste/bin ~/.local/bin
cp build-release/et build-release/etterminal ~/.local/et-image-paste/bin/
```

Recommended remote layout:

```sh
ssh l1 'mkdir -p ~/.local/et-image-paste/bin ~/.local/et-image-paste/run'
scp build-release/etserver build-release/etterminal \
  l1:~/.local/et-image-paste/bin/
```

On a Linux remote, build Linux `etserver` and `etterminal` for that remote
architecture before copying them.

## Wrapper Example

Create a wrapper instead of changing the system `et` command:

```sh
cat > ~/.local/bin/et-l1 <<'EOF'
#!/bin/sh
set -eu

LOCAL_HOME="${ET_IMAGE_PASTE_HOME:-$HOME/.local/et-image-paste}"
LOCAL_ET="$LOCAL_HOME/bin/et"
REMOTE_HOST="${ET_IMAGE_PASTE_HOST:-l1}"
REMOTE_PORT="${ET_IMAGE_PASTE_PORT:-20222}"
REMOTE_HOME="${ET_IMAGE_PASTE_REMOTE_HOME:-/home/l1/.local/et-image-paste}"
REMOTE_BIN="$REMOTE_HOME/bin"
REMOTE_RUN="$REMOTE_HOME/run"
LOCAL_TERM="${TERM:-xterm-256color}"

if [ "$LOCAL_TERM" = "dumb" ]; then
  LOCAL_TERM="xterm-256color"
fi

ssh "$REMOTE_HOST" "
  set -eu
  mkdir -p '$REMOTE_RUN'
  if ! (ss -ltn 2>/dev/null | grep -q ':$REMOTE_PORT '); then
    oldpid=\$(cat '$REMOTE_RUN/etserver.pid' 2>/dev/null || true)
    if [ -n \"\$oldpid\" ]; then
      kill \"\$oldpid\" 2>/dev/null || true
      sleep 1
    fi
    '$REMOTE_BIN/etserver' --port '$REMOTE_PORT' --daemon \
      --pidfile '$REMOTE_RUN/etserver.pid' --logdir '$REMOTE_RUN'
    sleep 1
  fi
"

exec env TERM="$LOCAL_TERM" "$LOCAL_ET" \
  -p "$REMOTE_PORT" \
  --terminal-path "$REMOTE_BIN/etterminal" \
  "$@" "$REMOTE_HOST"
EOF

chmod +x ~/.local/bin/et-l1
```

## Usage With Claude Code Or Codex

Start the patched ET wrapper:

```sh
et-l1
```

Then attach tmux or start the AI tool on the remote host:

```sh
tmux attach -t test
claude
```

Copy an image on the macOS client, focus the ET terminal, then press `Ctrl+V`.
The prompt should receive a path like:

```sh
/tmp/et-clipboard-images-1000/image-AbCd12.png
```

Press Enter after the path is inserted. The remote Claude/Codex process can read
that file because it exists on the remote host, not on the macOS client.

Plain `ssh l1` or `mosh l1` will not use this feature. The patched local `et`,
patched remote `etterminal`, and running remote `etserver` must all be used
together.

For a native-feeling `Cmd+V` on Ghostty, add:

```text
keybind = cmd+v=text:\x1b]777;et-paste\x07
```

Reload Ghostty. In patched ET, `Cmd+V` will read the macOS clipboard directly:
images become remote image paths, and text is pasted as text.

## Disable

For one session, disable local image interception:

```sh
ET_DISABLE_CLIPBOARD_IMAGE_PASTE=1 et-l1
```

For one session, disable the local clipboard paste trigger used by terminal
keybinds such as Ghostty `Cmd+V`:

```sh
ET_DISABLE_LOCAL_CLIPBOARD_PASTE_TRIGGER=1 et-l1
```

## Troubleshooting

Check that the wrapper is being used:

```sh
command -v et-l1
```

Check that the remote server is listening:

```sh
ssh l1 '(ss -ltnp 2>/dev/null || true) | grep 20222'
```

Check remote files after a paste:

```sh
ssh l1 'ls -lh /tmp/et-clipboard-images-$(id -u)/'
```

If pressing `Ctrl+V` only inserts `^V`, the remote `etterminal` probably did not
advertise `ETCAPS:clipboard-image-paste`, or the session was opened with an old
client/remote binary.

If no path appears, copy the image again on macOS and retry. Some apps put a file
URL or rich text on the clipboard instead of image bytes.
