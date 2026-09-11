#!/usr/bin/python3

# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2025 Raspberry Pi Ltd
#
# test_convert.py - Test script for libpisp convert utility
#

import argparse
import os
import subprocess
import sys
import hashlib


class ConvertTester:
    def __init__(
        self,
        convert_binary,
        output_dir=None,
        input_dir=None,
        reference_dir=None,
        use_gstreamer=False,
        gst_plugin_path=None,
    ):
        """Initialize the tester with the path to the convert binary."""
        self.convert_binary = convert_binary
        self.output_dir = output_dir
        self.input_dir = input_dir
        self.reference_dir = reference_dir
        self.use_gstreamer = use_gstreamer
        self.gst_plugin_path = gst_plugin_path

        if not use_gstreamer and not os.path.exists(convert_binary):
            raise FileNotFoundError(f"Convert binary not found: {convert_binary}")

        # Test cases: (input_file, output_file, input_format, output_format, reference_file)
        self.test_cases = [
            {
                "input_file": "conv_yuv420_4056x3040_4056s.yuv",
                "output_file": "out_4056x3040_12168s_rgb888.rgb",
                "input_format": "4056:3040:4056:YUV420P",
                "output_format": "4056:3040:12168:RGB888",
                "reference_file": "ref_4056x3040_12168s_rgb888.rgb",
                "skip_gst": False,
            },
            {
                "input_file": "conv_800x600_1200s_422_yuyv.yuv",
                "output_file": "out_1600x1200_422p.yuv",
                "input_format": "800:600:1600:YUYV",
                "output_format": "1600:1200:1600:YUV422P",
                "reference_file": "ref_1600x1200_1600_422p.yuv",
                "skip_gst": False,
            },
            {
                "input_file": "conv_rgb888_800x600_2432s.rgb",
                "output_file": "out_4000x3000_4032s.yuv",
                "input_format": "800:600:2432:RGB888",
                "output_format": "4000:3000:4032:YUV444P",
                "reference_file": "ref_4000x3000_4032s.yuv",
                "skip_gst": True,
            },
            # Strided inputs: the harness rewrites the input with padded rows,
            # passing the strides to rawvideoparse (exercising pispconvert's
            # GstVideoMeta stride handling) or to the convert utility via the
            # format string. Convert mode is skipped automatically if the
            # strides do not follow its luma/chroma derivation rules.
            {
                "input_file": "conv_yuv420_4056x3040_4056s.yuv",
                "output_file": "out_4056x3040_strided_rgb888.rgb",
                "input_format": "4056:3040:4056:YUV420P",
                "output_format": "4056:3040:12168:RGB888",
                "input_strides": [4160, 2080, 2080],
                "reference_file": "ref_4056x3040_12168s_rgb888.rgb",
            },
            {
                "input_file": "conv_800x600_1200s_422_yuyv.yuv",
                "output_file": "out_1600x1200_strided_422p.yuv",
                "input_format": "800:600:1600:YUYV",
                "output_format": "1600:1200:1600:YUV422P",
                "input_strides": [1728],
                "reference_file": "ref_1600x1200_1600_422p.yuv",
            },
            # RGB input with its native padded stride (2432 vs 800*3=2400).
            # The packed-stride output reference allows this to run in both
            # modes, unlike the 4032-stride variant above.
            {
                "input_file": "conv_rgb888_800x600_2432s.rgb",
                "output_file": "out_4000x3000_4000s.yuv",
                "input_format": "800:600:2432:RGB888",
                "output_format": "4000:3000:4000:YUV444P",
                "input_strides": [2432],
                "reference_file": "ref_4000x3000_4000s.yuv",
            },
            # BGR output variants exercise the pispconvert R/B swap CSC.
            # videoconvert repacks the file back to RGB so the RGB888
            # references can be reused; a wrong channel order in the element
            # shows up as a swapped file. GStreamer only.
            {
                "input_file": "conv_yuv420_4056x3040_4056s.yuv",
                "output_file": "out_4056x3040_bgr_rgb888.rgb",
                "input_format": "4056:3040:4056:YUV420P",
                "output_format": "4056:3040:12168:BGR",
                "file_format": "RGB",
                "reference_file": "ref_4056x3040_12168s_rgb888.rgb",
                "skip_convert": True,
            },
            {
                "input_file": "conv_yuv420_4056x3040_4056s.yuv",
                "output_file": "out_4056x3040_strided_bgr_rgb888.rgb",
                "input_format": "4056:3040:4056:YUV420P",
                "output_format": "4056:3040:12168:BGR",
                "input_strides": [4160, 2080, 2080],
                "file_format": "RGB",
                "reference_file": "ref_4056x3040_12168s_rgb888.rgb",
                "skip_convert": True,
            },
            # Add more test cases here as needed
        ]

    def _parse_format(self, format_str):
        """Parse format string like '4056:3040:4056:YUV420P' into components."""
        parts = format_str.split(":")
        if len(parts) != 4:
            raise ValueError(f"Invalid format string: {format_str}")
        return {
            "width": int(parts[0]),
            "height": int(parts[1]),
            "stride": int(parts[2]),
            "format": parts[3],
        }

    def _pisp_to_gst_format(self, pisp_format):
        """Convert PiSP format to GStreamer format string."""
        format_map = {
            "YUV420P": "I420",
            "YVU420P": "YV12",
            "YUV422P": "Y42B",
            "YUV444P": "Y444",
            "YUYV": "YUY2",
            "UYVY": "UYVY",
            "RGB888": "RGB",
        }
        return format_map.get(pisp_format, pisp_format)

    def _plane_geometry(self, fmt):
        """Per-plane (rows, row_bytes, stride) for a parsed format dict."""
        w, h, s = fmt["width"], fmt["height"], fmt["stride"]
        name = fmt["format"]
        if name in ("YUV420P", "YVU420P"):
            return [(h, w, s), (h // 2, w // 2, s // 2), (h // 2, w // 2, s // 2)]
        if name == "YUV422P":
            return [(h, w, s), (h, w // 2, s // 2), (h, w // 2, s // 2)]
        if name == "YUV444P":
            return [(h, w, s), (h, w, s), (h, w, s)]
        if name in ("YUYV", "UYVY"):
            return [(h, w * 2, s)]
        if name == "RGB888":
            return [(h, w * 3, s)]
        raise ValueError(f"Unsupported format for strided input: {name}")

    def _strided_input_path(self, input_file):
        """Output-dir path for the strided rewrite of an input file."""
        base = os.path.basename(input_file).removeprefix("conv_")
        return os.path.join(self.output_dir or ".", "conv_strided_" + base)

    def _make_strided_input(self, src_path, in_fmt, strides, dst_path):
        """Rewrite src_path with the given per-plane strides, zero-padding each
        row (including a final row the source file may have left unpadded).
        Returns the plane offsets of the new file."""
        planes = self._plane_geometry(in_fmt)
        if len(strides) != len(planes):
            raise ValueError("gst_input_strides needs one stride per plane")

        with open(src_path, "rb") as f:
            data = f.read()

        out = bytearray()
        offsets = []
        pos = 0
        for (rows, row_bytes, src_stride), dst_stride in zip(planes, strides):
            if dst_stride < row_bytes:
                raise ValueError(
                    f"Stride {dst_stride} smaller than row size {row_bytes}"
                )
            offsets.append(len(out))
            for _ in range(rows):
                row = data[pos : pos + row_bytes]
                out += row + b"\x00" * (dst_stride - len(row))
                pos += src_stride
        with open(dst_path, "wb") as f:
            f.write(out)
        return offsets

    def run_gstreamer(
        self,
        input_file,
        output_file,
        input_format,
        output_format,
        input_strides=None,
        file_format=None,
    ):
        """Run GStreamer pipeline with pispconvert."""
        # Use input directory if specified
        if self.input_dir:
            input_file = os.path.join(self.input_dir, input_file)

        # Use output directory if specified
        if self.output_dir:
            output_file = os.path.join(self.output_dir, output_file)

        # Parse format strings
        in_fmt = self._parse_format(input_format)
        out_fmt = self._parse_format(output_format)

        # Convert to GStreamer format names
        gst_in_format = self._pisp_to_gst_format(in_fmt["format"])
        gst_out_format = self._pisp_to_gst_format(out_fmt["format"])

        # Rewrite the input with explicit strides and tell rawvideoparse about
        # them, so pispconvert receives buffers with non-default GstVideoMeta
        parse_props = []
        if input_strides:
            strided_file = self._strided_input_path(input_file)
            offsets = self._make_strided_input(
                input_file, in_fmt, input_strides, strided_file
            )
            input_file = strided_file
            parse_props = [
                f"plane-strides=<{','.join(str(s) for s in input_strides)}>",
                f"plane-offsets=<{','.join(str(o) for o in offsets)}>",
            ]

        # Build GStreamer pipeline
        pipeline = [
            "gst-launch-1.0",
            "filesrc",
            f"location={input_file}",
            "!",
            "rawvideoparse",
            f"width={in_fmt['width']}",
            f"height={in_fmt['height']}",
            f"format={gst_in_format.lower()}",
            "framerate=30/1",
            *parse_props,
        ]

        # BT.601 colorimetry is only valid for YUV inputs; RGB keeps the
        # rawvideoparse default (identity matrix)
        if not in_fmt["format"].startswith("RGB"):
            pipeline += ["!", "video/x-raw,colorimetry=1:4:0:0"]

        pipeline += [
            "!",
            "pispconvert",
            "!",
            f"video/x-raw,format={gst_out_format},width={out_fmt['width']},height={out_fmt['height']},colorimetry=1:4:0:0",
        ]

        # Repack to the requested file format so a common reference can be used
        if file_format and file_format != gst_out_format:
            pipeline += [
                "!",
                "videoconvert",
                "!",
                f"video/x-raw,format={file_format},width={out_fmt['width']},height={out_fmt['height']}",
            ]

        pipeline += [
            "!",
            "filesink",
            f"location={output_file}",
        ]

        print("Running GStreamer pipeline:")
        print(" ".join(pipeline))

        # Set GST_PLUGIN_PATH environment variable if specified
        env = os.environ.copy()
        if self.gst_plugin_path:
            env["GST_PLUGIN_PATH"] = self.gst_plugin_path
            print(f"GST_PLUGIN_PATH={self.gst_plugin_path}")

        try:
            subprocess.run(
                pipeline, capture_output=True, text=True, check=True, env=env
            )
            print("GStreamer pipeline completed successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"GStreamer pipeline failed with exit code {e.returncode}")
            print(f"stdout: {e.stdout}")
            print(f"stderr: {e.stderr}")
            return False

    def run_convert(
        self, input_file, output_file, input_format, output_format, input_strides=None
    ):
        """Run the convert utility with the specified parameters."""
        # Use input directory if specified
        if self.input_dir:
            input_file = os.path.join(self.input_dir, input_file)

        # Use output directory if specified
        if self.output_dir:
            output_file = os.path.join(self.output_dir, output_file)

        # Rewrite the input with explicit strides and adjust the format
        # string accordingly (the caller has checked expressibility)
        if input_strides:
            in_fmt = self._parse_format(input_format)
            strided_file = self._strided_input_path(input_file)
            self._make_strided_input(input_file, in_fmt, input_strides, strided_file)
            input_file = strided_file
            input_format = (
                f"{in_fmt['width']}:{in_fmt['height']}:"
                f"{input_strides[0]}:{in_fmt['format']}"
            )

        cmd = [
            self.convert_binary,
            input_file,
            output_file,
            "--input-format",
            input_format,
            "--output-format",
            output_format,
        ]

        print(f"Running: {' '.join(cmd)}")

        try:
            subprocess.run(cmd, capture_output=True, text=True, check=True)
            print("Convert completed successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"Convert failed with exit code {e.returncode}")
            print(f"stdout: {e.stdout}")
            print(f"stderr: {e.stderr}")
            return False

    def compare_files(self, file1, file2):
        """Compare two files and return True if they are identical."""
        if not os.path.exists(file1):
            print(f"Error: File {file1} does not exist")
            return False
        if not os.path.exists(file2):
            print(f"Error: File {file2} does not exist")
            return False

        # Compare file sizes first
        size1 = os.path.getsize(file1)
        size2 = os.path.getsize(file2)

        if size1 != size2:
            print(f"Files have different sizes: {size1} vs {size2}")
            return False

        # Compare file contents using hash
        hash1 = self._file_hash(file1)
        hash2 = self._file_hash(file2)

        if hash1 == hash2:
            print("Files are identical")
            return True
        else:
            print("Files are different")
            return False

    def _file_hash(self, filepath):
        """Calculate SHA256 hash of a file."""
        hash_sha256 = hashlib.sha256()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()

    def run_test_case(self, test_case):
        """Run a single test case."""
        print("\n=== Running test case ===")
        print(f"Input file: {test_case['input_file']}")
        print(f"Output file: {test_case['output_file']}")
        print(f"Input format: {test_case['input_format']}")
        print(f"Output format: {test_case['output_format']}")
        print(f"Reference file: {test_case['reference_file']}")

        # Check if input file exists
        input_file = test_case["input_file"]
        if self.input_dir:
            input_file = os.path.join(self.input_dir, test_case["input_file"])

        if not os.path.exists(input_file):
            print(f"Error: Input file {input_file} does not exist")
            return False

        # Skip GStreamer test if marked to skip
        if self.use_gstreamer and test_case.get("skip_gst", False):
            print("SKIPPED: Test case marked as skip_gst=True")
            return None  # Return None to indicate skipped

        # Skip convert test if marked to skip
        if not self.use_gstreamer and test_case.get("skip_convert", False):
            print("SKIPPED: Test case marked as skip_convert=True")
            return None  # Return None to indicate skipped

        input_strides = test_case.get("input_strides")

        # The convert utility derives chroma strides from the luma stride, so
        # skip strides its format string cannot express
        if not self.use_gstreamer and input_strides:
            in_fmt = self._parse_format(test_case["input_format"])
            in_fmt["stride"] = input_strides[0]
            expected = [plane[2] for plane in self._plane_geometry(in_fmt)]
            if input_strides != expected:
                print("SKIPPED: strides not expressible by the convert utility")
                return None

        # Run the convert utility or GStreamer pipeline
        if self.use_gstreamer:
            success = self.run_gstreamer(
                test_case["input_file"],
                test_case["output_file"],
                test_case["input_format"],
                test_case["output_format"],
                input_strides,
                test_case.get("file_format"),
            )
        else:
            success = self.run_convert(
                test_case["input_file"],
                test_case["output_file"],
                test_case["input_format"],
                test_case["output_format"],
                input_strides,
            )

        if not success:
            return False

        # Compare with reference file if it exists
        reference_file = test_case["reference_file"]
        if self.reference_dir:
            reference_file = os.path.join(
                self.reference_dir, test_case["reference_file"]
            )

        if os.path.exists(reference_file):
            print("Comparing output with reference file...")
            # Use output directory for the generated output file
            output_file = test_case["output_file"]
            if self.output_dir:
                output_file = os.path.join(self.output_dir, test_case["output_file"])
            return self.compare_files(output_file, reference_file)
        else:
            print(f"Reference file {reference_file} not found")
            return False

    def run_all_tests(self):
        """Run all test cases."""
        if self.use_gstreamer:
            print("Testing with GStreamer pispconvert plugin")
            if self.gst_plugin_path:
                print(f"GST_PLUGIN_PATH: {self.gst_plugin_path}")
        else:
            print(f"Testing convert utility: {self.convert_binary}")
        if self.input_dir:
            print(f"Input directory: {self.input_dir}")
        if self.output_dir:
            print(f"Output directory: {self.output_dir}")
        if self.reference_dir:
            print(f"Reference directory: {self.reference_dir}")
        print(f"Number of test cases: {len(self.test_cases)}")

        passed = 0
        failed = 0
        skipped = 0

        for i, test_case in enumerate(self.test_cases, 1):
            print(f"\n--- Test case {i}/{len(self.test_cases)} ---")

            result = self.run_test_case(test_case)
            if result is None:
                skipped += 1
                print("⊘ Test SKIPPED")
            elif result:
                passed += 1
                print("✓ Test PASSED")
            else:
                failed += 1
                print("✗ Test FAILED")

        print("\n=== Test Summary ===")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print(f"Skipped: {skipped}")
        print(f"Total: {len(self.test_cases)}")

        return failed == 0


def main():
    parser = argparse.ArgumentParser(
        description="Test script for libpisp convert utility"
    )
    parser.add_argument(
        "convert_binary",
        nargs="?",
        default=None,
        help="Path to the convert binary (not needed with --gst-plugin-path)",
    )
    parser.add_argument("--test-dir", help="Directory containing test files")
    parser.add_argument(
        "--in", dest="input_dir", help="Directory containing input files"
    )
    parser.add_argument("--out", help="Directory where output files will be written")
    parser.add_argument("--ref", help="Directory containing reference files")
    parser.add_argument(
        "--gst-plugin-path",
        help="Path to GStreamer plugin directory (enables GStreamer testing)",
    )

    args = parser.parse_args()

    try:
        # Determine if using GStreamer based on --gst-plugin-path
        use_gstreamer = args.gst_plugin_path is not None

        # Validate arguments
        if not use_gstreamer and not args.convert_binary:
            parser.error(
                "convert_binary is required unless --gst-plugin-path is specified"
            )

        tester = ConvertTester(
            args.convert_binary,
            args.out,
            args.input_dir,
            args.ref,
            use_gstreamer=use_gstreamer,
            gst_plugin_path=args.gst_plugin_path,
        )

        # Change to test directory if specified
        if args.test_dir:
            if not os.path.exists(args.test_dir):
                print(f"Error: Test directory {args.test_dir} does not exist")
                return 1
            os.chdir(args.test_dir)
            print(f"Changed to test directory: {args.test_dir}")

        # Create output directory if specified and it doesn't exist
        if args.out:
            if not os.path.exists(args.out):
                os.makedirs(args.out)
                print(f"Created output directory: {args.out}")

        # Run all tests
        success = tester.run_all_tests()
        return 0 if success else 1

    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
