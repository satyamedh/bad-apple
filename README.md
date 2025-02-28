# Bad apple on the ESP32

## NOTE: You'll need at least 8mb of flash, something like the esp32-s3-n8r8 should work

The video is already encoded and stored in `main/video_data.h`

code used to encode the video is available in `convert_to_c_header.py`, the video should be resized to 128x64@12fps using ffmpeg.

Any video can be used in this manner, just resize using ffmpeg -> use the python script to encode to the C header -> copy paste into the file and recompile project.


This project uses [https://github.com/lexus2k/lcdgfx](lcdgfx), amazing library :D
