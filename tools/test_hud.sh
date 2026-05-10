#!/bin/sh
make hud_preview # This legit just automates the user running 3 commands.
./build/host/hud_preview "$@"
cmd //c start hud_preview.bmp # To run a file using the CMD you need to use "start".