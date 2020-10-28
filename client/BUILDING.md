## Building the client

The client depends on the Unix socket API and **libui**, the cross-platform GUI
library. To build the client, libui will need to be built and linked against.
The following shell commands demonstrate the way to do this on macOS (with
build tools **Meson** and **Ninja** installed). Please refer to the libui
documentation for details on building on other platforms.

All other code are written in C99 and should be portable to Linux.
In case of issues please consult related documentation or contact the author at
shiqing (hyphen) thu18 (at) yandex (dot) com.

```sh
cd libui
meson setup build --buildtype=release --default-library=static
ninja -C build
cd ..
# make
gcc -o client *.c -Wall -std=c99 libui/build/meson-out/libui.a -framework Cocoa -O2
```

## Note: preprocessing resources

Image resources need to be preprocessed into raw image data before building,
and processed data are not included in Git.

To complete this step, install ImageMagick command line tools (specifically,
**convert**), and run `res/gen.sh` before building.
