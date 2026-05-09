# wlr-image-desc-test

## Test app for `wp_color_management_surface_v1::set_image_description`

## Build

```bash
meson setup build/
ninja -C build
```

## Run

```bash
./build/main
```

Toggle HDR on and off.

Output should contain
```
output image description changed for DP-1!
```
followed by some basic TF and luminance info.