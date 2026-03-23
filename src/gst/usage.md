# PiSP Convert GStreamer Element

`pispconvert` is a GStreamer element that provides hardware-accelerated image scaling and format conversion using the Raspberry Pi's PiSP Backend.

## Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `output-buffer-count` | uint | 4 | Number of backend buffers to allocate (1-32) |
| `crop` | string | "0,0,0,0" | Crop region for all outputs as "x,y,width,height" |
| `crop0` | string | "0,0,0,0" | Crop region for output 0 as "x,y,width,height" |
| `crop1` | string | "0,0,0,0" | Crop region for output 1 as "x,y,width,height" |

### Crop Parameters

Crop values of `0,0,0,0` (default) means no cropping - the full input is used. If width or height is 0, it defaults to the full input dimension. Values are automatically clipped to fit within the input.

- `crop` - Sets the same crop region for both outputs
- `crop0` - Sets crop region for output 0 only
- `crop1` - Sets crop region for output 1 only


## Supported Formats

### GStreamer Formats
`RGB`, `RGBx`, `BGRx`, `I420`, `YV12`, `Y42B`, `Y444`, `YUY2`, `UYVY`, `NV12_128C8`, `NV12_10LE32_128C8`

### DRM Formats
`RG24`, `XB24`, `XR24`, `YU12`, `YV12`, `YU16`, `YU24`, `YUYV`, `UYVY`, `NV12`, `NV12:0x0700000000000004`, `P030:0x0700000000000004`

## Colorimetry

`pispconvert` uses the colorimetry reported in the input caps to select the correct
YCbCr conversion matrix. The supported colour spaces are: `jpeg` (full-range BT.601),
`smpte170m` (limited-range BT.601), `rec709`, `rec709_full`, `bt2020`, and `bt2020_full`.

When no output colorimetry is specified, it defaults to matching the input.

Some webcams and V4L2 sources report incorrect colorimetry (e.g. limited-range when the
sensor actually produces full-range data), which can result in incorrect colours. You can
override the input colorimetry by specifying it explicitly in the caps filter. The
colorimetry string format is `range:matrix:transfer:primaries`.

For example, to force full-range BT.601 (jpeg) on a webcam:

```bash
gst-launch-1.0 \
    v4l2src device=/dev/video0 io-mode=dmabuf ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=640,height=480,colorimetry=1:4:0:1" ! \
    pispconvert ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=800,height=600" ! \
    waylandsink
```

You can check the actual colorimetry of your camera with:

```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
```

## Examples

### Single Output

Basic scaling and format conversion:

```bash
gst-launch-1.0 filesrc location=input.yuv ! \
    rawvideoparse width=4056 height=3040 format=i420 framerate=30/1 ! \
    pispconvert ! \
    video/x-raw,format=RGB,width=1920,height=1080 ! \
    filesink location=output.rgb
```

### Dual Output

Simultaneous scaling to two different resolutions/formats:

```bash
gst-launch-1.0 filesrc location=input.yuv ! \
    rawvideoparse width=4056 height=3040 format=i420 framerate=30/1 ! \
    pispconvert name=p \
    p.src0 ! queue ! video/x-raw,format=RGB,width=1920,height=1080 ! filesink location=output0.rgb \
    p.src1 ! queue ! video/x-raw,format=I420,width=640,height=480 ! filesink location=output1.yuv
```

### With Cropping

Crop the input before scaling:

```bash
gst-launch-1.0 filesrc location=input.yuv ! \
    rawvideoparse width=4056 height=3040 format=i420 framerate=30/1 ! \
    pispconvert crop="500,400,3000,2200" ! \
    video/x-raw,format=RGB,width=1920,height=1080 ! \
    filesink location=output.rgb
```

### Camera with DMABuf

Using a camera source with DMABuf for zero-copy processing:

```bash
gst-launch-1.0 \
    v4l2src device=/dev/video16 io-mode=dmabuf num-buffers=100 ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=640,height=480" ! \
    pispconvert ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=4096,height=1080" ! \
    waylandsink sync=false
```

### Camera with Software Buffers

Using a camera source with standard memory buffers:

```bash
gst-launch-1.0 \
    v4l2src device=/dev/video16 num-buffers=100 ! \
    "video/x-raw,width=640,height=480" ! \
    pispconvert ! \
    video/x-raw,format=RGB,width=4096,height=1080 ! \
    waylandsink
```

### Decode H.265 Video and Reformat

Using hardware H.265 decoder with DMABuf passthrough:

```bash
gst-launch-1.0 filesrc location=video.mkv ! matroskademux ! h265parse ! v4l2slh265dec ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=NV12:0x0700000000000004" ! \
    pispconvert ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=4096,height=1080" ! \
    waylandsink
```

Using software buffer input instead of DMABuf:

```bash
gst-launch-1.0 filesrc location=video.mkv ! matroskademux ! h265parse ! v4l2slh265dec ! \
    "video/x-raw,format=NV12_128C8" ! \
    pispconvert ! \
    "video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YUYV,width=4096,height=1080" ! \
    waylandsink
```
