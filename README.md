# libpisp

A helper library to generate run-time configuration for the Raspberry Pi ISP (PiSP), consisting of the Frontend and Backend hardware components.

## Building and installing
To build, setup the meson project as follows:

```sh
meson setup <build_dir>
```
To optionally disable the Boost logging library, add ``-Dlogging=disabled`` as an argument to the ``meson setup`` command.

To compile and install the ``libpisp.so`` artefact:
```sh
meson compile -C <build_dir>
sudo meson install -C <build_dir>
```

## Linking libpisp with an application
libpisp can be built and linked as a [meson subproject](https://mesonbuild.com/Subprojects.html) by using an appropriate [libpisp.wrap](utils/libpisp.wrap) file and the following dependency declaration in the target project:
```meson
libpisp_dep = dependency('libpisp', fallback : ['libpisp', 'libpisp_dep'])
```

Alternatively [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) can be used to locate ``libpisp.so`` installed in of the system directories for other build environments.

## Command-Line Tools

### convert

A simple command-line image converter that uses the PiSP Backend for hardware-accelerated format conversion and scaling.

```sh
pisp_convert input.yuv output.rgb \
    --input-format 1920:1080:1920:YUV420P \
    --output-format 1280:720:3840:RGB888
```

Format strings use the form `width:height:stride:format`. Use `--formats` to list available formats, or `--list` to enumerate available PiSP devices.

## GStreamer Element

libpisp includes a GStreamer element (`pispconvert`) that provides hardware-accelerated image scaling and format conversion using the PiSP Backend.

### Building with GStreamer Support

GStreamer support is enabled by default if the required dependencies are found. To explicitly enable or disable:

```sh
meson setup <build_dir> -Dgstreamer=enabled   # require GStreamer support
meson setup <build_dir> -Dgstreamer=disabled  # disable GStreamer support
```

### Using the Element

After installation, the element will be available as `pispconvert`:

```sh
gst-inspect-1.0 pispconvert
```

For usage examples and supported formats, see [src/gst/usage.md](src/gst/usage.md).

### Testing Without Installing

To test the plugin without installing:

```sh
GST_PLUGIN_PATH=<build_dir>/src/gst gst-inspect-1.0 pispconvert
```

## License
Copyright © 2023, Raspberry Pi Ltd. Released under the BSD-2-Clause License.
