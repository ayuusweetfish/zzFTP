```sh
cd libui
meson setup build --buildtype=release --default-library=static
ninja -C build
cd ..
# make
gcc *.c -Wall -std=c99 libui/build/meson-out/libui.a -framework Cocoa
```
