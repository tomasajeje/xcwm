#ifndef XCWM_H
#define XCWM_H

#include <X11/Xlib.h>

#define MODE_TILING  0
#define MODE_PANNING 1

typedef struct Client {
    Window w;
    int cx, cy;
    unsigned int ww, wh;
    int tag;
    struct Client *next;
} Client;

typedef union {
    int i;
    const char **v;
} Arg;

extern Display *d;
extern Window root;
extern Client *list, *cur;
extern int sw, sh, current_tag;
extern int mode[4];
extern int vx[4], vy[4];
extern Client *fullscreen_client;

extern Atom wm_protocols;
extern Atom wm_delete_window;
extern Atom net_active_window;
extern Atom net_wm_state;
extern Atom net_wm_state_fullscreen;
extern Atom wm_take_focus;

void spawn(const Arg *arg);
void kill_client(const Arg *arg);
void toggle_mode(const Arg *arg);
void center_focused(const Arg *arg);
void pan_client(const Arg *arg);
void toggle_fullscreen(const Arg *arg);
void cycle_client(const Arg *arg);
void move_client(const Arg *arg);
void tag_client(const Arg *arg);
void change_tag(const Arg *arg);
void arrange(void);
void focus(Client *c);
void quit(const Arg *arg);
int  get_window_type(Window w);
void update_ewmh_atoms(void);
void set_wm_state(Window w, long state);

#endif
