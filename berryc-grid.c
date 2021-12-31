#include <X11/X.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cjson/cJSON.h>

/* Difference between _NET_ACTIVE_WINDOW and XGetInputFocus:
 * https://stackoverflow.com/questions/31800880/xlib-difference-between-net-active-window-and-xgetinputfocus
 *
 * XGetInputFocus() also seems to return the wrong window for sakura, so it 
 * does seem to be the case that _NET_ACTIVE_WINDOW is more robust.
 */

struct Rect {
  int x, y, width, height;
};
struct Geometry {
  struct Rect window, screen;
};

void rect_from_json_obj(cJSON *json, struct Rect *rect) {
  while (json) {
    if (cJSON_IsNumber(json)) {
      if (strcmp(json->string, "x") == 0) {
        rect->x = json->valueint;
      }
      else if (strcmp(json->string, "y") == 0) {
        rect->y = json->valueint;
      }
      else if (strcmp(json->string, "width") == 0) {
        rect->width = json->valueint;
      }
      else if (strcmp(json->string, "height") == 0) {
        rect->height = json->valueint;
      }
    }
    json = json->next;
  }
}

void geom_from_json_root(cJSON *root, struct Geometry *geom) {
  cJSON *json = root->child;

  while (json) {
    if (cJSON_IsObject(json)) {
      if (strcmp(json->string, "geom") == 0) {
        rect_from_json_obj(json->child, &geom->window);
      }
      else if (strcmp(json->string, "monitor") == 0) {
        rect_from_json_obj(json->child, &geom->screen);
      }
    }
    json = json->next;
  }
}

int get_active_window(Display *display, Window root, Window *active_window) {
  Atom NET_ACTIVE_WINDOW = XInternAtom(display, "_NET_ACTIVE_WINDOW", 1);

  Atom actual_type;
  int actual_format;
  unsigned long num_items, bytes_remaining;
  unsigned char *raw_property;
  int status;

  status = XGetWindowProperty(
    display,
    root, 
    NET_ACTIVE_WINDOW,
    0,                      // read offset
    sizeof(Window),         // read length
    0,                      // delete?
    XA_WINDOW,              // required type
    &actual_type,
    &actual_format,
    &num_items,
    &bytes_remaining,
    &raw_property
  );
  if (status != Success) {
    return 1;
  }

  *active_window = *((Window *) raw_property);
  return 0;
}

int get_env_int(const char *name, int default_) {
  char *str = getenv(name);
  return str? atoi(str) : default_;
}

int get_berry_window_status(Display *display, Window window, char **json_str) {
  Atom BERRY_WINDOW_STATUS = XInternAtom(display, "BERRY_WINDOW_STATUS", 1);
  Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", 1);

  Atom actual_type;
  int actual_format;
  unsigned long num_items, bytes_remaining;
  unsigned char *raw_property;
  int status;

  // Use fixed read length of 1024 bytes.  There could be more data than this, 
  // in which case another call to XGetWindowProperty() would be necessary.  
  // For now, I'm taking the fixed-length approach in the interest of 
  // simplicity.

  status = XGetWindowProperty(
    display,
    window,
    BERRY_WINDOW_STATUS,
    0,                      // read offset
    1024,                   // read length
    0,                      // delete?
    UTF8_STRING,            // required type
    &actual_type,
    &actual_format,
    &num_items,
    &bytes_remaining,
    &raw_property
  );
  if (status != Success) {
    return 1;
  }
  if (bytes_remaining) {
    return 2;
  }

  *json_str = ((char *) raw_property);
  return 0;
}

int main(int argc, char **argv) {
  Display *display = 0;
  int status;
  
  display = XOpenDisplay(0);
  if (!display) {
    printf("FATAL: failed to connect to X11 server");
    return 1;
  }

  Window root = XDefaultRootWindow(display);
  Window active_window;
  status = get_active_window(display, root, &active_window);
  if (status != 0) {
    printf("FATAL: failed to identify active window");
    return 1;
  }

  char *json_str;
  status = get_berry_window_status(display, active_window, &json_str);
  if (status != 0) {
    printf("FATAL: failed to load BERRY_WINDOW_STATUS");
    return 1;
  }

  cJSON *json = cJSON_Parse(json_str);
  if (! json) {
    printf("FATAL: error parsing JSON: %s", json_str);
    printf("FATAL: is berry the current window manager?");
    return 1;
  }

  struct Geometry geom;
  geom_from_json_root(json, &geom);

  if (argc != 5) {
    printf("Usage: berryc-grid <left> <right> <top> <bottom>");
    return 1;
  }

  int left = atoi(argv[1]);
  int right = atoi(argv[2]);
  int top = atoi(argv[3]);
  int bottom = atoi(argv[4]);

  int BERRY_GRID_ROWS = get_env_int("BERRY_GRID_ROWS", 2);
  int BERRY_GRID_COLUMNS = get_env_int("BERRY_GRID_COLUMNS", 4);
  int BERRY_GRID_CELL_WIDTH = get_env_int("BERRY_GRID_CELL_WIDTH", 650);
  int BERRY_GRID_CELL_HEIGHT = get_env_int("BERRY_GRID_CELL_HEIGHT", 371);
  int BERRY_GRID_FILL_EDGES = get_env_int("BERRY_GRID_FILL_EDGES", 1);

  if (right < 0) {
    right += BERRY_GRID_COLUMNS + 1;
  }
  if (bottom < 0) {
    bottom += BERRY_GRID_ROWS + 1;
  }

  int x = left * BERRY_GRID_CELL_WIDTH;
  int y = top * BERRY_GRID_CELL_HEIGHT;
  int w = (right - left) * BERRY_GRID_CELL_WIDTH;
  int h = (bottom - top) * BERRY_GRID_CELL_HEIGHT;

  if (BERRY_GRID_FILL_EDGES) {
    if (bottom == BERRY_GRID_ROWS) {
      h = geom.screen.height - top * BERRY_GRID_CELL_HEIGHT;
    }
    if (right == BERRY_GRID_COLUMNS) {
      w = geom.screen.width - left * BERRY_GRID_CELL_WIDTH;
    }
  }

  if (x + w > geom.screen.width) {
    w = geom.screen.width - x;
  }
  if (y + h > geom.screen.height) {
    h = geom.screen.height - y;
  }

  // If both arguments were 5 digits, the resize command would be 41 chars 
  // long.  So these 50-char buffers should be safe.
  char cmd_resize[50], cmd_move[50];

  sprintf(cmd_resize, "berryc window_resize_absolute %d %d", w, h);
  sprintf(cmd_move,   "berryc window_move_absolute %d %d", x, y);

  system(cmd_resize);
  system(cmd_move);
}

