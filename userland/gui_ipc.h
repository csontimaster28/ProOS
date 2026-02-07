// IPC protocol definitions between displayd/gfxd/apps
#ifndef GUI_IPC_H
#define GUI_IPC_H

#include <stdint.h>

// Message types
#define GUI_MSG_CREATE_SURFACE 1
#define GUI_MSG_DESTROY_SURFACE 2
#define GUI_MSG_DRAW 3
#define GUI_MSG_INPUT 4
#define GUI_MSG_FOCUS 5

typedef struct {
    uint32_t type;
    uint32_t src_pid;
    uint32_t dest_pid;
    uint32_t size;
    uint8_t data[0];
} gui_ipc_msg_t;

// create surface request
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t flags;
} gui_create_surface_t;

// draw request: primitive drawing commands encoded by gfxd
typedef struct {
    uint32_t surface_id;
    uint32_t cmd; // 1=pixel,2=rect,3=blit
    uint32_t x,y,w,h,color;
    uint32_t payload_len;
} gui_draw_t;

// input event
typedef struct {
    uint32_t event_type; // 1=mouse,2=key
    int32_t x,y;
    uint32_t keycode;
    uint32_t pressed;
} gui_input_t;

#endif // GUI_IPC_H
