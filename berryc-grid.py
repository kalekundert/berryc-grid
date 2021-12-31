#!/usr/bin/env python3

"""\
Issue berryc commands to resize a window to a grid.

Usage:
    berryc-grid <left> <right> <top> <bottom> 
"""

import sys, os
import json

from Xlib.display import Display
from subprocess import run

x11 = Display()

NET_ACTIVE_WINDOW = x11.intern_atom('_NET_ACTIVE_WINDOW')
BERRY_WINDOW_STATUS = x11.intern_atom('BERRY_WINDOW_STATUS')

root = x11.screen().root
window_id = root.get_full_property(NET_ACTIVE_WINDOW, 0).value[0]
window = x11.create_resource_object('window', window_id)
status_str = window.get_full_property(BERRY_WINDOW_STATUS, 0).value
status = json.loads(status_str)

# Environment/config variables
BERRY_GRID_ROWS = 2
BERRY_GRID_COLUMNS = 4
BERRY_GRID_CELL_WIDTH = 650
BERRY_GRID_CELL_HEIGHT = 371
BERRY_GRID_FILL_EDGES = True

try:
    left, right, top, bottom = map(int, sys.argv[1:5])
except ValueError:
    print("Usage: berryc-grid <left> <right> <top> <bottom>")
    raise SystemExit(1) from None

mw = status['monitor']['width']
mh = status['monitor']['height']

if right < 0:
    right += BERRY_GRID_COLUMNS + 1
if bottom < 0:
    bottom += BERRY_GRID_ROWS + 1

x = left * BERRY_GRID_CELL_WIDTH
y = top * BERRY_GRID_CELL_HEIGHT
w = (right - left) * BERRY_GRID_CELL_WIDTH
h = (bottom - top) * BERRY_GRID_CELL_HEIGHT

if BERRY_GRID_FILL_EDGES:
    if bottom == BERRY_GRID_ROWS:
        h = mh - top * BERRY_GRID_CELL_HEIGHT
    if right == BERRY_GRID_COLUMNS:
        w = mw - left * BERRY_GRID_CELL_WIDTH

if x + w > mw:
    w = mw - x
if y + h > mh:
    h = mh - y

#run(['berryc', 'window_resize_absolute', str(w), str(h)])
#run(['berryc', 'window_move_absolute', str(x), str(y)])

