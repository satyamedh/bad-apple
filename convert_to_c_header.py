#!/usr/bin/env python3
import cv2
import numpy as np
import sys


def rle_encode(data):
    """Run-length encode a string of '0's and '1's with a colon separator."""
    if not data:
        return ""

    encoded = []
    prev_char = data[0]
    count = 1
    for char in data[1:]:
        if char == prev_char:
            count += 1
        else:
            encoded.append(f"{count}:{prev_char}")
            prev_char = char
            count = 1
    # Append the last run
    encoded.append(f"{count}:{prev_char}")
    return " ".join(encoded)


def main():
    input_video = "output5.avi"
    output_header = "video_data.h"

    # Open the video file
    cap = cv2.VideoCapture(input_video)
    if not cap.isOpened():
        print("Error: Could not open video file.")
        sys.exit(1)

    # We'll build the bit string for the entire video.
    # If the video is large, consider processing frame-by-frame and handling run boundaries.
    bit_chars = []  # using a list for efficiency

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # If the frame is in color, convert it to grayscale.
        if len(frame.shape) == 3 and frame.shape[2] == 3:
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # Threshold to ensure the frame is binary.
        # Any pixel > 128 becomes 1; otherwise, 0.
        _, binary = cv2.threshold(frame, 128, 1, cv2.THRESH_BINARY)

        # Flatten the 2D binary frame into a 1D array.
        flat = binary.flatten().astype(np.uint8)

        # Convert each pixel to its string representation ('0' or '1') and extend our list.
        bit_chars.extend(str(pixel) for pixel in flat)

    cap.release()

    # Combine all bits into a single string.
    bit_string = "".join(bit_chars)

    # Run-length encode the bit string.
    encoded = rle_encode(bit_string)

    # Create the C++ header file with the encoded string.
    header_contents = (
        "#ifndef VIDEO_DATA_H\n"
        "#define VIDEO_DATA_H\n\n"
        f"const char* video_data = \"{encoded}\";\n\n"
        "#endif // VIDEO_DATA_H\n"
    )

    with open(output_header, "w") as f:
        f.write(header_contents)

    print(f"Header file '{output_header}' generated successfully.")


if __name__ == "__main__":
    main()
