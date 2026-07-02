#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include "xcwm.h"
#include "config.h"

static unsigned int numlockmask = 0;
#define CLEANMASK(mask) (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

Display *d;
Window   root;
Client  *list = NULL, *cur = NULL;
int      sw, sh;
int      current_tag = 1;
int      running     = 1;

int mode[4] = { MODE_TILING, MODE_TILING, MODE_TILING, MODE_TILING };
int vx[4]   = { 0, 0, 0, 0 };
int vy[4]   = { 0, 0, 0, 0 };

int      drag_mode = 0;
int      start_x = 0, start_y = 0;
int      start_cx = 0, start_cy = 0;
unsigned int start_w = 0, start_h = 0;
int      start_vx = 0, start_vy = 0;
Client  *drag_client = NULL;
int      tdrag_x = 0, tdrag_y = 0;

Client  *fullscreen_client = NULL;

Atom wm_protocols;
Atom wm_delete_window;
Atom net_active_window;
Atom net_wm_state;
Atom net_wm_state_fullscreen;
Atom wm_take_focus;
Atom net_supported;
Atom net_supporting_wm_check;
Window wm_check_win;
Time   last_time = CurrentTime; /* timestamp del ultimo evento real */
int    bar_top    = 0;  /* y donde termina la barra superior (detectado) */
int    bar_bottom = 0;  /* altura reservada por barras inferiores */

/* ── helpers ────────────────────────────────────────────────── */

static void update_struts(void) {
    bar_top = 0; bar_bottom = 0;
    Window dw; unsigned int nchildren;
    Window *children = NULL;
    if (!XQueryTree(d, root, &dw, &dw, &children, &nchildren)) return;

    Atom strut_p = XInternAtom(d, "_NET_WM_STRUT_PARTIAL", True);
    Atom strut   = XInternAtom(d, "_NET_WM_STRUT",         True);

    for (unsigned int k = 0; k < nchildren; k++) {
        Atom actual_type; int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;

        int ok = 0;
        if (strut_p != None &&
            XGetWindowProperty(d, children[k], strut_p, 0, 12, False,
                               XA_CARDINAL, &actual_type, &actual_format,
                               &nitems, &bytes_after, &prop) == Success
            && prop && nitems >= 4) {
            unsigned long *s = (unsigned long *)prop;
            /* s[2]=top s[3]=bottom */
            if ((int)s[2] > bar_top)    bar_top    = (int)s[2];
            if ((int)s[3] > bar_bottom) bar_bottom  = (int)s[3];
            ok = 1;
        }
        if (prop) { XFree(prop); prop = NULL; }

        /* Fallback: _NET_WM_STRUT (4 valores: left right top bottom) */
        if (!ok && strut != None &&
            XGetWindowProperty(d, children[k], strut, 0, 4, False,
                               XA_CARDINAL, &actual_type, &actual_format,
                               &nitems, &bytes_after, &prop) == Success
            && prop && nitems >= 4) {
            unsigned long *s = (unsigned long *)prop;
            if ((int)s[2] > bar_top)    bar_top    = (int)s[2];
            if ((int)s[3] > bar_bottom) bar_bottom  = (int)s[3];
        }
        if (prop) { XFree(prop); prop = NULL; }
    }
    if (children) XFree(children);
}

static void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int xerror(Display *dpy, XErrorEvent *ee) { (void)dpy; (void)ee; return 0; }

void updatenumlockmask(void) {
    unsigned int i, j;
    XModifierKeymap *modmap = XGetModifierMapping(d);
    numlockmask = 0;
    for(i = 0; i < 8; i++)
        for(j = 0; j < (unsigned int)modmap->max_keypermod; j++)
            if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(d, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

static void close_window(Window w) {
    Atom *protocols = NULL;
    int n = 0, has_delete = 0;
    if (XGetWMProtocols(d, w, &protocols, &n)) {
        for (int i = 0; i < n; i++)
            if (protocols[i] == wm_delete_window) { has_delete = 1; break; }
        XFree(protocols);
    }
    if (has_delete) {
        XEvent ev = {0};
        ev.type                 = ClientMessage;
        ev.xclient.window       = w;
        ev.xclient.message_type = wm_protocols;
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = (long)wm_delete_window;
        ev.xclient.data.l[1]    = CurrentTime;
        XSendEvent(d, w, False, NoEventMask, &ev);
    } else {
        XKillClient(d, w);
    }
}

static void get_pointer(int *px, int *py) {
    Window dw; int di; unsigned int du;
    XQueryPointer(d, root, &dw, &dw, px, py, &di, &di, &du);
}

void set_wm_state(Window w, long state) {
    long data[] = { state, None };
    Atom wm_state = XInternAtom(d, "WM_STATE", False);
    XChangeProperty(d, w, wm_state, wm_state, 32, PropModeReplace, (unsigned char *)data, 2);
}

void update_ewmh_atoms(void) {
    Atom net_number_of_desktops = XInternAtom(d, "_NET_NUMBER_OF_DESKTOPS", False);
    Atom net_current_desktop    = XInternAtom(d, "_NET_CURRENT_DESKTOP",    False);
    unsigned long num_desktops  = 3;
    unsigned long cur_desktop   = (unsigned long)(current_tag - 1);
    XChangeProperty(d, root, net_number_of_desktops, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&num_desktops, 1);
    XChangeProperty(d, root, net_current_desktop, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&cur_desktop, 1);
    XFlush(d);
}

static void update_net_active_window(Window w) {
    XChangeProperty(d, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

int get_window_type(Window w) {
    Atom actual_type; int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    Atom net_wm_window_type = XInternAtom(d, "_NET_WM_WINDOW_TYPE", True);
    if (net_wm_window_type == None) return 0;

    /* Tipos de ventana que el WM debe ignorar completamente:
     * no se tilan, no reciben foco, no se registran como clientes. */
    Atom ignore_types[] = {
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_BAR",          True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_NOTIFICATION", True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_TOOLTIP",      True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_SPLASH",       True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_POPUP_MENU",   True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_COMBO",        True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_DND",          True),
        XInternAtom(d, "_NET_WM_WINDOW_TYPE_DESKTOP",      True),
    };
    int ntypes = sizeof(ignore_types) / sizeof(ignore_types[0]);

    if (XGetWindowProperty(d, w, net_wm_window_type, 0, 32, False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *atoms = (Atom *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            for (int j = 0; j < ntypes; j++) {
                if (ignore_types[j] != None && atoms[i] == ignore_types[j]) {
                    XFree(prop);
                    return 1;
                }
            }
        }
        XFree(prop);
    }
    return 0;
}

void spawn(const Arg *arg) {
    if (fork() == 0) {
        if (d) close(ConnectionNumber(d));
        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        exit(0);
    }
}

void quit(const Arg *arg) { (void)arg; running = 0; }

void kill_client(const Arg *arg) {
    (void)arg;
    if (!cur) return;
    if (fullscreen_client == cur) fullscreen_client = NULL;
    close_window(cur->w);
}

/* ── focus ──────────────────────────────────────────────────── */

void focus(Client *c) {
    if (!c) {
        cur = NULL;
        XSetInputFocus(d, root, RevertToPointerRoot, CurrentTime);
        update_net_active_window(None);
        return;
    }
    if (c->tag != current_tag) return;
    cur = (fullscreen_client && fullscreen_client->tag == current_tag) ? fullscreen_client : c;

    /* Leer WM_HINTS: si InputHint=0 el cliente maneja su propio foco (Alacritty).
     * En ese caso NO llamamos XSetInputFocus, solo mandamos WM_TAKE_FOCUS. */
    int do_set_focus = 1;
    XWMHints *hints = XGetWMHints(d, cur->w);
    if (hints) {
        if ((hints->flags & InputHint) && !hints->input)
            do_set_focus = 0;
        XFree(hints);
    }
    if (do_set_focus)
        XSetInputFocus(d, cur->w, RevertToPointerRoot, last_time);

    /* WM_TAKE_FOCUS: mandarlo siempre que el cliente lo soporte,
     * con el timestamp real del ultimo evento (no CurrentTime). */
    Atom *protocols = NULL;
    int n = 0, has_take_focus = 0;
    if (XGetWMProtocols(d, cur->w, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm_take_focus) { has_take_focus = 1; break; }
        }
        XFree(protocols);
    }

    if (has_take_focus) {
        XEvent ev = {0};
        ev.type                 = ClientMessage;
        ev.xclient.window       = cur->w;
        ev.xclient.message_type = wm_protocols;
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = (long)wm_take_focus;
        ev.xclient.data.l[1]    = (long)last_time;
        XSendEvent(d, cur->w, False, NoEventMask, &ev);
    }

    XRaiseWindow(d, cur->w);
    update_net_active_window(cur->w);

    for (Client *i = list; i; i = i->next)
        if (i->tag == current_tag)
            XSetWindowBorder(d, i->w, (i == cur) ? 0x999999 : 0x444444);
}

/* ── tiling slot positions ──────────────────────────────────── */

static void get_slot_center(int slot, int n, int *cx, int *cy) {
    int ty      = bar_top + GAP;
    int area_h  = sh - bar_top - bar_bottom - GAP * 2;
    int ns      = (n > 1) ? n - 1 : 1;
    int master_w = (n == 1) ? sw - GAP * 2 : sw / 2 - GAP - GAP / 2;
    int stack_w  = sw - master_w - GAP * 3;
    int stack_h  = (n > 1) ? (area_h - GAP * (ns - 1)) / ns : 0;

    if (slot == 0) {
        *cx = GAP + (master_w - 4) / 2;
        *cy = ty + (area_h - 4) / 2;
    } else {
        int sy = ty + (slot - 1) * (stack_h + GAP);
        *cx = master_w + GAP * 2 + (stack_w - 4) / 2;
        *cy = sy + (stack_h - 4) / 2;
    }
}

static int nearest_slot(int px, int py, int n) {
    int best = 0;
    int best_dist = 0x7fffffff;
    for (int s = 0; s < n; s++) {
        int cx, cy;
        get_slot_center(s, n, &cx, &cy);
        int dx = px - cx, dy = py - cy;
        int dist = dx*dx + dy*dy;
        if (dist < best_dist) { best_dist = dist; best = s; }
    }
    return best;
}

static void move_to_slot(Client *c, int target_slot) {
    Client **tc;
    for (tc = &list; *tc && *tc != c; tc = &(*tc)->next);
    if (!*tc) return;
    *tc = c->next;
    c->next = NULL;

    int n = 0;
    for (Client *i = list; i; i = i->next)
        if (i->tag == current_tag) n++;

    if (target_slot > n) target_slot = n;

    Client *same[256]; int sc = 0;
    Client *other_head = NULL, **other_tail = &other_head;
    Client *cur_node = list;
    list = NULL;
    while (cur_node) {
        Client *next = cur_node->next;
        cur_node->next = NULL;
        if (cur_node->tag == current_tag && sc < 256)
            same[sc++] = cur_node;
        else {
            *other_tail = cur_node;
            other_tail  = &cur_node->next;
        }
        cur_node = next;
    }

    if (target_slot > sc) target_slot = sc;
    for (int i = sc; i > target_slot; i--)
        same[i] = same[i-1];
    same[target_slot] = c;
    sc++;

    list = NULL;
    Client **tail = &list;
    for (int i = 0; i < sc; i++) {
        *tail = same[i];
        tail  = &same[i]->next;
    }
    *tail = other_head;
}

void arrange(void) {
    int n = 0, i = 0;
    Client *c;

    for (c = list; c; c = c->next) {
        if (c->tag != current_tag) {
            XMoveWindow(d, c->w, sw * 2, sh * 2);
        } else {
            XSetWindowBorderWidth(d, c->w, (fullscreen_client == c) ? 0 : 2);
            n++;
        }
    }

    if (n == 0) { cur = NULL; return; }

    if (fullscreen_client && fullscreen_client->tag == current_tag) {
        for (c = list; c; c = c->next)
            if (c->tag == current_tag && c != fullscreen_client)
                XMoveWindow(d, c->w, sw * 2, sh * 2);
        XMoveResizeWindow(d, fullscreen_client->w, 0, 0, sw, sh);
        return;
    }

    if (mode[current_tag] == MODE_TILING) {
        vx[current_tag] = 0; vy[current_tag] = 0;

        /* Área usable: entre bar_top y (sh - bar_bottom), con gaps */
        int ty      = bar_top + GAP;                        /* y inicial */
        int area_h  = sh - bar_top - bar_bottom - GAP * 2; /* altura total */
        int ns      = (n > 1) ? n - 1 : 1;                 /* ventanas en stack */
        int master_w = (n == 1) ? sw - GAP * 2 : sw / 2 - GAP - GAP / 2;
        int stack_w  = sw - master_w - GAP * 3;
        int stack_h  = (n > 1) ? (area_h - GAP * (ns - 1)) / ns : 0;

        for (c = list; c; c = c->next) {
            if (c->tag != current_tag) continue;

            if (drag_mode == 4 && drag_client && c == drag_client) {
                XMoveResizeWindow(d, c->w, tdrag_x, tdrag_y, c->ww, c->wh);
                XRaiseWindow(d, c->w);
                i++;
                continue;
            }

            if (i == 0) {
                /* Master: toda la altura usable */
                XMoveResizeWindow(d, c->w,
                    GAP, ty,
                    (unsigned int)(master_w - 4),
                    (unsigned int)(area_h   - 4));
            } else {
                /* Stack */
                int sy = ty + (i - 1) * (stack_h + GAP);
                XMoveResizeWindow(d, c->w,
                    master_w + GAP * 2, sy,
                    (unsigned int)(stack_w - 4),
                    (unsigned int)(stack_h - 4));
            }
            i++;
        }
    } else {
        for (c = list; c; c = c->next) {
            if (c->tag != current_tag) continue;
            XMoveResizeWindow(d, c->w,
                (sw / 2) + (c->cx - vx[current_tag]),
                (sh / 2) + (c->cy - vy[current_tag]),
                c->ww, c->wh);
        }
    }
}

void toggle_mode(const Arg *arg) {
    (void)arg;
    if (fullscreen_client && fullscreen_client->tag == current_tag) return;
    mode[current_tag] = (mode[current_tag] == MODE_TILING) ? MODE_PANNING : MODE_TILING;
    if (mode[current_tag] == MODE_PANNING) {
        vx[current_tag] = 0; vy[current_tag] = 0;
        if (cur) { cur->cx = -((int)cur->ww / 2); cur->cy = -((int)cur->wh / 2); }
        int step = 1, dir = 0;
        const int spacing = 60;
        for (Client *c = list; c; c = c->next) {
            if (c->tag != current_tag || c == cur) continue;
            if (c->ww < 100 || c->wh < 100) { c->ww = 640; c->wh = 440; }
            if (cur) {
                switch (dir % 4) {
                    case 0: c->cx = cur->cx + (int)cur->ww + spacing; c->cy = cur->cy; break;
                    case 1: c->cx = cur->cx - (int)c->ww - spacing;  c->cy = cur->cy; break;
                    case 2: c->cx = cur->cx; c->cy = cur->cy + (int)cur->wh + spacing; break;
                    case 3: c->cx = cur->cx; c->cy = cur->cy - (int)cur->wh - spacing;  break;
                }
            } else {
                c->cx = ((dir % 2 == 0) ? 200 : -200) * step - (int)(c->ww / 2);
                c->cy = ((dir / 2 == 0) ? 200 : -200) * step - (int)(c->wh / 2);
            }
            dir++;
            if (dir % 4 == 0) step++;
        }
    }
    arrange();
}

void center_focused(const Arg *arg) {
    (void)arg;
    if (mode[current_tag] != MODE_PANNING || !cur) return;
    vx[current_tag] = cur->cx + (int)(cur->ww / 2);
    vy[current_tag] = cur->cy + (int)(cur->wh / 2);
    arrange();
}

/* Mueve la ventana enfocada y la camara juntas. arg->i: 0=R 1=L 2=D 3=U */
void pan_client(const Arg *arg) {
    if (mode[current_tag] != MODE_PANNING || !cur) return;
    int dx = 0, dy = 0;
    switch (arg->i) {
        case 0: dx =  PAN_STEP; break;
        case 1: dx = -PAN_STEP; break;
        case 2: dy =  PAN_STEP; break;
        case 3: dy = -PAN_STEP; break;
    }
    cur->cx += dx;  cur->cy += dy;
    vx[current_tag] += dx;  vy[current_tag] += dy;
    arrange();
}

void toggle_fullscreen(const Arg *arg) {
    (void)arg;
    if (!cur) return;
    if (fullscreen_client == cur) {
        fullscreen_client = NULL;
        XChangeProperty(d, cur->w, net_wm_state, XA_ATOM, 32, PropModeReplace, NULL, 0);
    } else {
        fullscreen_client = cur;
        XChangeProperty(d, cur->w, net_wm_state, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_state_fullscreen, 1);
    }
    arrange(); focus(cur);
}

void cycle_client(const Arg *arg) {
    if (fullscreen_client && fullscreen_client->tag == current_tag) return;
    if (!cur || !list->next) return;
    Client *c = cur;
    do {
        if (arg->i > 0) c = c->next ? c->next : list;
        else {
            Client *prev = list;
            while (prev->next && prev->next != c) prev = prev->next;
            c = prev;
        }
    } while (c->tag != current_tag && c != cur);
    focus(c);
    if (mode[current_tag] == MODE_PANNING) {
        vx[current_tag] = cur->cx + (int)(cur->ww / 2);
        vy[current_tag] = cur->cy + (int)(cur->wh / 2);
    }
    arrange();
}

void move_client(const Arg *arg) {
    if (!cur || !list || !list->next) return;
    if (fullscreen_client && fullscreen_client->tag == current_tag) return;

    Client *other = NULL;

    if (arg->i > 0) {
        for (other = cur->next; other; other = other->next)
            if (other->tag == current_tag) break;
        if (!other) {
            for (other = list; other; other = other->next)
                if (other->tag == current_tag) break;
        }
    } else {
        Client *last = NULL;
        for (Client *i = list; i; i = i->next) {
            if (i->tag == current_tag) {
                if (i == cur) other = last;
                last = i;
            }
        }
        if (!other) other = last;
    }

    if (!other || other == cur) return;

    Window tw = cur->w;
    int tcx = cur->cx, tcy = cur->cy;
    unsigned int tww = cur->ww, twh = cur->wh;

    cur->w = other->w;
    cur->cx = other->cx;
    cur->cy = other->cy;
    cur->ww = other->ww;
    cur->wh = other->wh;

    other->w = tw;
    other->cx = tcx;
    other->cy = tcy;
    other->ww = tww;
    other->wh = twh;

    cur = other;
    focus(cur);
    arrange();
}

void tag_client(const Arg *arg) {
    if (!cur || arg->i == current_tag) return;
    cur->tag = arg->i;

    Client *next_focus = NULL;
    for (Client *c = list; c; c = c->next) {
        if (c->tag == current_tag) {
            next_focus = c;
            break;
        }
    }
    focus(next_focus);
    arrange();
}

void change_tag(const Arg *arg) {
    if (arg->i == current_tag) return;
    current_tag = arg->i;
    update_ewmh_atoms();
    Client *target = NULL;
    for (Client *c = list; c; c = c->next)
        if (c->tag == current_tag) { target = c; break; }
    arrange();
    if (target) focus(target);
    else {
        if (!(fullscreen_client && fullscreen_client->tag == current_tag)) cur = NULL;
        XSetInputFocus(d, PointerRoot, RevertToPointerRoot, CurrentTime);
        update_net_active_window(None);
    }
}

/* ── event handlers ─────────────────────────────────────────── */

void propertynotify(XEvent *e) {
    XPropertyEvent *ev = &e->xproperty;
    Atom strut_p = XInternAtom(d, "_NET_WM_STRUT_PARTIAL", False);
    Atom strut   = XInternAtom(d, "_NET_WM_STRUT", False);

    if (ev->atom == strut_p || ev->atom == strut) {
        update_struts();
        arrange();
    }
}

void keypress(XEvent *e) {
    last_time = e->xkey.time;
    KeySym keysym = XLookupKeysym(&e->xkey, 0);
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(e->xkey.state))
            keys[i].func(&(keys[i].arg));
}

void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    wc.x = ev->x; wc.y = ev->y;
    wc.width = ev->width; wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above; wc.stack_mode = ev->detail;
    XConfigureWindow(d, ev->window, (unsigned int)ev->value_mask, &wc);
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;
    XWindowAttributes wa;
    XGetWindowAttributes(d, ev->window, &wa);

    if (wa.override_redirect || get_window_type(ev->window)) {
        XSelectInput(d, ev->window, PropertyChangeMask);
        XMapWindow(d, ev->window);
        /* Puede ser una barra — actualizar struts y reorganizar */
        XSync(d, False);
        update_struts();
        arrange();
        return;
    }

    for (Client *c = list; c; c = c->next)
        if (c->w == ev->window) return;

    Client *c = calloc(1, sizeof(Client));
    c->w   = ev->window;
    c->tag = current_tag;
    c->ww  = (wa.width  > 100) ? (unsigned int)wa.width  : 800;
    c->wh  = (wa.height > 100) ? (unsigned int)wa.height : 600;

    if (mode[current_tag] == MODE_PANNING) {
        int px, py;
        get_pointer(&px, &py);
        c->cx = vx[current_tag] + (px - (sw / 2)) - (c->ww / 2);
        c->cy = vy[current_tag] + (py - (sh / 2)) - (c->wh / 2);
        c->next = list; list = c;
    } else {
        c->next = list;
        list = c;
    }

    XSetWindowBorderWidth(d, c->w, 3);
    set_wm_state(c->w, 1);

    XSelectInput(d, c->w, FocusChangeMask | EnterWindowMask | StructureNotifyMask);
    XMapWindow(d, c->w);
    arrange();
    /* El foco lo da mapnotify() cuando la ventana ya es Viewable */
}

/* MapNotify: la ventana terminó de mapearse, ahora es seguro darle foco */
void mapnotify(XEvent *e) {
    XMapEvent *ev = &e->xmap;
    
    /* Interceptar Polybar (override_redirect) cuando termina de mapearse */
    XWindowAttributes wa;
    if (XGetWindowAttributes(d, ev->window, &wa) && wa.override_redirect) {
        XSelectInput(d, ev->window, PropertyChangeMask);
        update_struts();
        arrange();
        return;
    }

    for (Client *c = list; c; c = c->next) {
        if (c->w == ev->window && c->tag == current_tag) {
            focus(c);
            break;
        }
    }
}

void destroynotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client **tc;
    for (tc = &list; *tc && (*tc)->w != ev->window; tc = &(*tc)->next);
    if (!*tc) return;
    Client *unmapped = *tc;
    *tc = unmapped->next;
    if (cur == unmapped) {
        Client *next_focus = NULL;
        for (Client *c = list; c; c = c->next)
            if (c->tag == current_tag) { next_focus = c; break; }
        focus(next_focus);
    }
    if (fullscreen_client == unmapped) fullscreen_client = NULL;
    free(unmapped);
    arrange();
}

void clientmessage(XEvent *e) {
    XClientMessageEvent *ev = &e->xclient;
    if (ev->message_type == net_wm_state) {
        if ((Atom)ev->data.l[1] == net_wm_state_fullscreen ||
            (Atom)ev->data.l[2] == net_wm_state_fullscreen) {
            for (Client *c = list; c; c = c->next)
                if (c->w == ev->window) { focus(c); toggle_fullscreen(NULL); break; }
        }
    } else if (ev->message_type == net_active_window) {
        for (Client *c = list; c; c = c->next)
            if (c->w == ev->window && c->tag == current_tag) { focus(c); break; }
    }
}

void enternotify(XEvent *e) {
    if (fullscreen_client && fullscreen_client->tag == current_tag) return;
    XCrossingEvent *ev = &e->xcrossing;
    if (ev->mode != NotifyNormal || ev->detail == NotifyInferior) return;
    for (Client *c = list; c; c = c->next)
        if (c->w == ev->window && c->tag == current_tag) { focus(c); break; }
}

void buttonpress(XEvent *e) {
    if (fullscreen_client && fullscreen_client->tag == current_tag) return;
    XButtonEvent *ev = &e->xbutton;
    last_time = ev->time;

    /* Identificar el cliente: el evento viene del grab en root,
     * así que la ventana real del cliente está en ev->subwindow. */
    Client *c = NULL;
    Window target = (ev->subwindow != None) ? ev->subwindow : ev->window;
    for (Client *i = list; i; i = i->next)
        if (i->tag == current_tag && i->w == target) { c = i; break; }

    /* Sin MOD: no debería llegar aquí (no tenemos grab en clientes),
     * pero si llega (clic en root sin ventana debajo) simplemente lo ignoramos. */
    if (!(CLEANMASK(ev->state) == MOD || CLEANMASK(ev->state) == (MOD | ShiftMask))) {
        XAllowEvents(d, ReplayPointer, CurrentTime);
        XFlush(d);
        return;
    }

    /* Forzar foco con MOD+clic sobre una ventana */
    if (c && c != cur) focus(c);

    /* Panning: MOD+Shift+clic3 → pan del canvas */
    if (mode[current_tag] == MODE_PANNING && ev->button == 3
        && CLEANMASK(ev->state) == (MOD | ShiftMask)) {
        drag_mode = 3;
        start_x = ev->x_root; start_y = ev->y_root;
        start_vx = vx[current_tag]; start_vy = vy[current_tag];
        XGrabPointer(d, root, False, PointerMotionMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        return;
    }

    if (!c) return;

    if (mode[current_tag] == MODE_PANNING) {
        drag_client = c;
        drag_mode = (ev->button == 1) ? 1 : 2;
        start_x = ev->x_root; start_y = ev->y_root;
        start_cx = c->cx; start_cy = c->cy;
        start_w = c->ww; start_h = c->wh;
    } else {
        drag_client = c;
        if (ev->button == 1) {
            drag_mode = 4;
            tdrag_x = ev->x_root - (int)(c->ww / 2);
            tdrag_y = ev->y_root - (int)(c->wh / 2);
        } else if (ev->button == 3) {
            drag_mode = 2;
            start_x = ev->x_root; start_y = ev->y_root;
            start_w = c->ww; start_h = c->wh;
        }
    }
    XGrabPointer(d, root, False, PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

void motionnotify(XEvent *e) {
    if (!drag_mode) return;
    XMotionEvent *ev = &e->xmotion;
    int dx = ev->x_root - start_x;
    int dy = ev->y_root - start_y;

    if (drag_mode == 1 && drag_client) {
        drag_client->cx = start_cx + dx;
        drag_client->cy = start_cy + dy;
        arrange();
    } else if (drag_mode == 2 && drag_client) {
        drag_client->ww = (unsigned int)((int)start_w + dx > 50 ? (int)start_w + dx : 50);
        drag_client->wh = (unsigned int)((int)start_h + dy > 50 ? (int)start_h + dy : 50);
        arrange();
    } else if (drag_mode == 3) {
        vx[current_tag] = start_vx - dx;
        vy[current_tag] = start_vy - dy;
        arrange();
    } else if (drag_mode == 4 && drag_client) {
        tdrag_x = ev->x_root - (int)(drag_client->ww / 2);
        tdrag_y = ev->y_root - (int)(drag_client->wh / 2);
        arrange();
    }
}

void buttonrelease(XEvent *e) {
    (void)e;
    if (!drag_mode) return;
    XUngrabPointer(d, CurrentTime);

    if (drag_mode == 4 && drag_client) {
        int px, py;
        get_pointer(&px, &py);

        int n = 0;
        for (Client *c = list; c; c = c->next)
            if (c->tag == current_tag) n++;

        int slot = nearest_slot(px, py, n);
        move_to_slot(drag_client, slot);
        focus(drag_client);
    }

    drag_mode   = 0;
    drag_client = NULL;
    arrange();
}

/* ── main ───────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    if (!(d = XOpenDisplay(NULL))) return 1;

    signal(SIGCHLD, sigchld_handler);
    XSetErrorHandler(xerror);

    root = DefaultRootWindow(d);
    sw = DisplayWidth(d, DefaultScreen(d));
    sh = DisplayHeight(d, DefaultScreen(d));

    wm_protocols            = XInternAtom(d, "WM_PROTOCOLS", False);
    wm_delete_window        = XInternAtom(d, "WM_DELETE_WINDOW", False);
    net_active_window       = XInternAtom(d, "_NET_ACTIVE_WINDOW", False);
    net_wm_state            = XInternAtom(d, "_NET_WM_STATE", False);
    net_wm_state_fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    wm_take_focus           = XInternAtom(d, "WM_TAKE_FOCUS", False);

    /* EWMH para Alacritty/Winit y clientes modernos */
    net_supported           = XInternAtom(d, "_NET_SUPPORTED", False);
    net_supporting_wm_check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False);

    wm_check_win = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(d, wm_check_win, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);
    XChangeProperty(d, wm_check_win, XInternAtom(d, "_NET_WM_NAME", False),
                    XInternAtom(d, "UTF8_STRING", False), 8,
                    PropModeReplace, (unsigned char *)"xcwm", 4);
    XChangeProperty(d, root, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);

    Atom supported[] = {
        net_supported, net_active_window, net_wm_state, net_wm_state_fullscreen,
        net_supporting_wm_check, wm_delete_window, wm_take_focus
    };
    XChangeProperty(d, root, net_supported, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)supported, 7);

    /*
     * SubstructureNotifyMask → recibir MapNotify de ventanas hijas del root.
     * Sin esto el MapNotify nunca llega al event loop principal.
     */
    XSelectInput(d, root,
                 SubstructureRedirectMask | SubstructureNotifyMask);

    updatenumlockmask();

    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        KeyCode kc = XKeysymToKeycode(d, keys[i].keysym);
        if (!kc) continue; /* keysym no existe en este teclado, saltar */
        for (unsigned int j = 0; j < 4; j++)
            XGrabKey(d, kc, keys[i].mod | modifiers[j],
                     root, False, GrabModeAsync, GrabModeAsync);
    }

    XGrabButton(d, 1, MOD,           root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(d, 3, MOD,           root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(d, 3, MOD|ShiftMask, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    Cursor cursor = XCreateFontCursor(d, XC_left_ptr);
    XDefineCursor(d, root, cursor);
    update_ewmh_atoms();
    
    /* Buscar paneles existentes antes de calcular los struts iniciales */
    Window dw_tree, *children = NULL;
    unsigned int nchildren;
    if (XQueryTree(d, root, &dw_tree, &dw_tree, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            XWindowAttributes wa;
            if (XGetWindowAttributes(d, children[i], &wa) && wa.override_redirect) {
                XSelectInput(d, children[i], PropertyChangeMask);
            }
        }
        if (children) XFree(children);
    }
    
    update_struts();

    XEvent e;
    while (running && !XNextEvent(d, &e)) {
        switch (e.type) {
            case KeyPress:         keypress(&e);         break;
            case MapRequest:       maprequest(&e);       break;
            case MapNotify:        mapnotify(&e);        break;
            case ConfigureRequest: configurerequest(&e); break;
            case ClientMessage:    clientmessage(&e);    break;
            case DestroyNotify:    destroynotify(&e);    break;
            case ButtonPress:      buttonpress(&e);      break;
            case EnterNotify:      enternotify(&e);      break;
            case MotionNotify:     motionnotify(&e);     break;
            case ButtonRelease:    buttonrelease(&e);    break;
            case PropertyNotify:   propertynotify(&e);   break;
        }
    }
    XCloseDisplay(d);
    return 0;
}
