#ifndef CONFIG_H
#define CONFIG_H

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#define MOD        Mod4Mask
#define GAP        4

static const char *termcmd[]    = { "st", NULL };
static const char *roficmd[] = { "/usr/bin/rofi", "-show", "drun", NULL };
static const char *volup[]      = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "5%+", NULL };
static const char *voldown[]    = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "5%-", NULL };
static const char *volmute[]    = { "wpctl", "set-mute",   "@DEFAULT_AUDIO_SINK@", "toggle", NULL };
static const char *brightup[]   = { "brightnessctl", "set", "5%+", NULL };
static const char *brightdown[] = { "brightnessctl", "set", "5%-", NULL };
static const char *wallpicker[] = { "/home/tomasa/.local/bin/wallpaper-picker", NULL };

typedef struct {
    unsigned int mod;
    KeySym       keysym;
    void       (*func)(const Arg *);
    const Arg    arg;
} Key;

#define PAN_STEP 40

static Key keys[] = {
    { MOD,            XK_q,      spawn,             {.v = termcmd}  },
    { MOD,            XK_d,      spawn,             {.v = roficmd} },
    { MOD,            XK_w,      kill_client,       {0}             },
    { MOD,            XK_v,      toggle_mode,       {0}             },
    { MOD,            XK_c,      center_focused,    {0}             },
    { MOD,            XK_f,      toggle_fullscreen, {0}             },
    { MOD,            XK_j,      cycle_client,      {.i =  1}       },
    { MOD,            XK_k,      cycle_client,      {.i = -1}       },
    { MOD|ShiftMask,  XK_j,      move_client,       {.i =  1}       },
    { MOD|ShiftMask,  XK_k,      move_client,       {.i = -1}       },
    { MOD, XK_p, spawn, {.v = wallpicker} },

    /* Panning: mover ventana+camara con flechas (0=R 1=L 2=D 3=U) */
    { MOD|ShiftMask,  XK_Right,  pan_client,        {.i = 0}        },
    { MOD|ShiftMask,  XK_Left,   pan_client,        {.i = 1}        },
    { MOD|ShiftMask,  XK_Down,   pan_client,        {.i = 2}        },
    { MOD|ShiftMask,  XK_Up,     pan_client,        {.i = 3}        },
    { MOD,            XK_1,      change_tag,        {.i = 1}        },
    { MOD,            XK_2,      change_tag,        {.i = 2}        },
    { MOD,            XK_3,      change_tag,        {.i = 3}        },
    { MOD|ShiftMask,  XK_1,      tag_client,        {.i = 1}        },
    { MOD|ShiftMask,  XK_2,      tag_client,        {.i = 2}        },
    { MOD|ShiftMask,  XK_3,      tag_client,        {.i = 3}        },
    { MOD|ShiftMask,  XK_e,      quit,              {0}             },
    { 0, XF86XK_AudioRaiseVolume,  spawn, {.v = volup}      },
    { 0, XF86XK_AudioLowerVolume,  spawn, {.v = voldown}    },
    { 0, XF86XK_AudioMute,         spawn, {.v = volmute}    },
    { 0, XF86XK_MonBrightnessUp,   spawn, {.v = brightup}   },
    { 0, XF86XK_MonBrightnessDown, spawn, {.v = brightdown} },
};

#endif
