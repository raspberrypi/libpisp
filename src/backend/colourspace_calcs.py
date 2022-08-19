# Reference https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-007.html

from curses.ascii import isdigit, isspace
import numpy as np

enc_601 = np.array([
    [0.299,     0.587,      0.114],
    [-0.169,    -0.331,     0.5],
    [0.5,       -0.419,     -0.081]
])
inv_601 = np.linalg.inv(enc_601)

enc_709 = np.array([
    [0.2126,    0.7152,     0.0722],
    [-0.1146,   -0.3854,    0.5],
    [0.5,       -0.4542,    -0.0458]
])
inv_709 = np.linalg.inv(enc_709)

enc_sycc = np.array([
    [0.2990,    0.5870,     0.1140],
    [-0.1687,   -0.3313,    0.5],
    [0.5,       -0.4187,    -0.0813]
])
inv_sycc = np.linalg.inv(enc_sycc)

enc_bt2020 = np.array([
    [0.2627,    0.6780,     0.0593],
    [-0.1396,   -0.3604,    0.5],
    [0.5,       -0.4598,    -0.0402]
])
inv_bt2020 = np.linalg.inv(enc_bt2020)

limited_scaling = np.diag([(235-16)/255, (240-16)/255, (240-16)/255])

limited_scaling_rgb = np.diag([(235-16)/255, (235-16)/255, (235-16)/255])

inv_limited_scaling = np.linalg.inv(limited_scaling)

inv_limited_scaling_rgb = np.linalg.inv(limited_scaling_rgb)

offsets = np.array([
    0, 128, 128
])

limited_offsets = np.array([
    16, 128, 128
])


def floats_to_ints(a):
    a = np.copy(a)
    a *= 2**10
    a = np.rint(a).astype(int)
    a = a.flatten().tolist()
    return a


def floats_to_offsets(a):
    a = np.copy(a)
    a *= 2**(26 - 8)
    a = np.rint(a).astype(int)
    a = a.flatten().tolist()
    return a


colour_encoding = {
    "select": "default",
    "default": {
        "ycbcr": {
            "coeffs": [306, 601, 116, -173, -338, 512, 512, -429, -82],
            "offsets": [0, 33554432, 33554432]
        },
        "ycbcr_inverse": {
            "coeffs": [1024, 0, 1435, 1024, -353, -731, 1024, 1813, 0],
            "offsets": [-47054848, 35520512, -59441152]
        }
    },
    # JPEG is enc_sycc with full range
    "jpeg": {
        "ycbcr": {
            "coeffs": floats_to_ints(enc_sycc),
            "offsets": floats_to_offsets(offsets)
        },
        "ycbcr_inverse": {
            "coeffs": floats_to_ints(inv_sycc),
            "offsets": floats_to_offsets(-inv_sycc.dot(offsets))
        }
    },

    # SMPTE 170M is enc_601 with limited range
    "smpte170m": {
        "ycbcr": {
            "coeffs": floats_to_ints(limited_scaling @ enc_601),
            "offsets": floats_to_offsets(limited_offsets)
        },
        "ycbcr_inverse": {
            "coeffs": floats_to_ints(inv_601 @ inv_limited_scaling),
            "offsets": floats_to_offsets(-(inv_601 @ inv_limited_scaling).dot(limited_offsets))
        }
    },

    # Rec 709 is enc_709 with limited range
    "rec709": {
        "ycbcr": {
            "coeffs": floats_to_ints(limited_scaling @ enc_709),
            "offsets": floats_to_offsets(limited_offsets)
        },
        "ycbcr_inverse": {
            "coeffs": floats_to_ints(inv_709 @ inv_limited_scaling),
            "offsets": floats_to_offsets(-(inv_709 @ inv_limited_scaling).dot(limited_offsets))
        }
    },

    # BT2020 is enc_bt2020 with limited range YCbCr and RGB
    "bt2020": {
        "ycbcr": {
            "coeffs": floats_to_ints(limited_scaling @ enc_bt2020 @ inv_limited_scaling_rgb),
            "offsets": floats_to_offsets(limited_offsets)
        },
        "ycbcr_inverse": {
            "coeffs": floats_to_ints(limited_scaling_rgb @ inv_bt2020 @ inv_limited_scaling),
            "offsets": floats_to_offsets(-(limited_scaling_rgb @ inv_bt2020 @ inv_limited_scaling).dot(limited_offsets))
        }
    }
}


if __name__ == "__main__":
    import json
    data = json.dumps(colour_encoding, indent=4)
    in_number = False
    new_line_loc = 0
    out = ""
    for i in range(len(data)):
        if data[i] == '\n':
            line = data[new_line_loc:i]
            if in_number:
                line = line.strip('\n')
                line = line.strip()
                if line[-1] == ',':
                    line += " "
                in_number = False
            out += line
            new_line_loc = i
        elif i == len(data) - 1:
            line = data[new_line_loc:]
            out += line
        if data[i].isdigit() or data[i] == '-' or data[i] == ']':
            if data[i-1].isspace():
                in_number = True

    print(out)
    print()
    print("Copy the above into the colour_encoding section of the json file")
