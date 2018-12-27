# wf-sound-control

A small GUI utility to control sound volume, for use with the wayfire wayland compositor.
Dependencies: `gtkmm`, `alsa`, `wayland-client` and `wayland-protocols`.

```
git clone https://github.com/WayfireWM/wf-sound-control && cd wf-sound-control
meson build --prefix=/usr --buildtype=release
ninja -C build && sudo ninja -C build install
```

# Usage

`wf-sound-control` (no arguments) - show/update visual indication
`wf-sound-control inc 5` - increase sound volume by 5 percent & show/update visual indication
`wf-sound-control dec 5` - decrease sound volume by 5 percent & show/update visual indication
