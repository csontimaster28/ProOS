#include "process.h"
#include "ipc.h"
#include "event.h"
#include "logging.h"
#include "capability.h"
#include "framebuffer.h"

// Forward declarations for daemon entry points
void procd_main(void);
void logd_main(void);
void fsd_main(void);
void netd_main(void);
void aidad_main(void);
// Kernel-space GUI daemons (MVP)
void gfxd_main(void);
void displayd_main(void);
void hello_gui_main(void);

// initd: starts other daemons
void initd_main(void) {
    // Assign capabilities for daemons
    uint32_t procd_caps = CAP_PROC_CONTROL | CAP_INTROSPECT;
    uint32_t fsd_caps = CAP_FS_READ | CAP_FS_WRITE;
    uint32_t logd_caps = CAP_INTROSPECT;
    uint32_t netd_caps = CAP_NET_SEND | CAP_NET_RECV;
    uint32_t aidad_caps = CAP_INTROSPECT;

    uint32_t pid_procd = process_create(procd_main, 4096, "procd", procd_caps);
    uint32_t pid_fsd = process_create(fsd_main, 4096, "fsd", fsd_caps);
    uint32_t pid_logd = process_create(logd_main, 4096, "logd", logd_caps);
    uint32_t pid_netd = process_create(netd_main, 4096, "netd", netd_caps);
    uint32_t pid_aidad = process_create(aidad_main, 4096, "aidad", aidad_caps);
    // Try to spawn userland GUI daemons from ELF if present, otherwise fall back to kernel-space renderers
    if (fs_exists("/bin/gfxd.elf")) {
        process_create_elf("/bin/gfxd.elf", 8192, "gfxd", CAP_INTROSPECT);
    } else {
        process_create(gfxd_main, 8192, "gfxd", CAP_INTROSPECT);
    }
    if (fs_exists("/bin/displayd.elf")) {
        process_create_elf("/bin/displayd.elf", 8192, "displayd", CAP_INTROSPECT);
    } else {
        process_create(displayd_main, 8192, "displayd", CAP_INTROSPECT);
    }
    if (fs_exists("/bin/hello_gui.elf")) {
        process_create_elf("/bin/hello_gui.elf", 4096, "hello_gui", CAP_INTROSPECT);
    } else {
        process_create(hello_gui_main, 4096, "hello_gui", CAP_INTROSPECT);
    }
    (void)pid_procd; (void)pid_fsd; (void)pid_logd; (void)pid_netd; (void)pid_aidad;
    log_info("initd: spawned procd/fsd/logd/netd/aidad");

    // Simple supervisor loop
    while (1) {
        // In a real initd we'd monitor child processes and restart them.
        asm volatile ("hlt");
    }
}

// Simple GUI input state (cursor)
int gui_cursor_x = 60;
int gui_cursor_y = 60;
static int folder_counter = 1;
static int gui_filemgr_active = 0;

void filemgr_main(void) {
    // Simple kernel-space file manager window
    framebuffer_info_t info; framebuffer_get_info(&info);
    uint32_t width = info.width; uint32_t height = info.height;
    while (1) {
        uint8_t *back = framebuffer_get_back();
        if (!back) { asm volatile("hlt"); continue; }
        // Window rectangle
        uint32_t wx = 160, wy = 60, ww = 400, wh = 300;
        for (uint32_t y = wy; y < wy+wh && y < height; y++) {
            for (uint32_t x = wx; x < wx+ww && x < width; x++) {
                uint32_t off = y * info.pitch + x * (info.bpp/8);
                back[off+0] = 0xEE; back[off+1] = 0xEE; back[off+2] = 0xEE; back[off+3] = 0xFF;
            }
        }
        // New Folder button
        uint32_t bx = wx + 12, by = wy + 12, bw = 120, bh = 28;
        for (uint32_t y = by; y < by+bh && y < height; y++) for (uint32_t x = bx; x < bx+bw && x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0x44; back[off+1] = 0x44; back[off+2] = 0x44; back[off+3] = 0xFF;
        }
        // Draw list of files from root
        inode_t *root = &inode_table[0];
        int line = 0;
        for (int i = 0; i < 16; i++) {
            uint32_t ci = root->children[i];
            if (ci == 0) continue;
            if (!inode_table[ci].is_used) continue;
            // Draw a small bar per file
            uint32_t lx = wx + 12; uint32_t ly = wy + 52 + line*20; uint32_t lw = ww - 24; uint32_t lh = 16;
            for (uint32_t y = ly; y < ly+lh && y < height; y++) for (uint32_t x = lx; x < lx+lw && x < width; x++) {
                uint32_t off = y * info.pitch + x * (info.bpp/8);
                back[off+0] = 0xAA; back[off+1] = 0xAA; back[off+2] = 0xAA; back[off+3] = 0xFF;
            }
            line++;
            if (line >= 10) break;
        }
        framebuffer_swap();
        asm volatile("hlt");
    }
}

void gui_move_cursor(int dx, int dy) {
    framebuffer_info_t info; framebuffer_get_info(&info);
    int nx = gui_cursor_x + dx; int ny = gui_cursor_y + dy;
    if (nx < 0) nx = 0; if (ny < 0) ny = 0;
    if (nx >= (int)info.width) nx = info.width - 1;
    if (ny >= (int)info.height) ny = info.height - 1;
    gui_cursor_x = nx; gui_cursor_y = ny;
}

void gui_click(void) {
    // Icons are at (40 + i*80, 40) size 56x56 for i=0..2
    for (int i = 0; i < 3; i++) {
        int ix = 40 + i*80; int iy = 40; int iw = 56; int ih = 56;
        if (gui_cursor_x >= ix && gui_cursor_x < ix+iw && gui_cursor_y >= iy && gui_cursor_y < iy+ih) {
            if (i == 0) {
                // Terminal icon -> toggle console
                extern void keyboard_toggle_console(void);
                keyboard_toggle_console();
            } else if (i == 1) {
                // New folder icon -> create a folder in cwd
                char name[32]; int n = folder_counter++;
                // simple name
                name[0] = 'F'; name[1] = 'o'; name[2] = 'l'; name[3] = 'd'; name[4] = 'e'; name[5] = 'r'; name[6] = '\0';
                // append number (very simple)
                char numbuf[8]; itoa(n, numbuf);
                int pos = 6; for (int j = 0; numbuf[j] && pos < 30; j++) name[pos++] = numbuf[j]; name[pos] = '\0';
                fs_create_folder(name);
            } else if (i == 2) {
                // Launch kernel-space file manager
                gui_filemgr_active = 1;
                process_create(filemgr_main, 8192, "filemgr", CAP_INTROSPECT);
            }
        }
    }
}

// procd: process tracker (placeholder)
void procd_main(void) {
    uint32_t mypid = process_get_current_pid();
    // Subscribe to kernel events
    event_subscribe(mypid);
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_message(mypid, &msg) == 0) {
            // For now, log raw event
            log_info("procd: received event");
        }
        asm volatile ("hlt");
    }
}

// logd: consumes kernel event bus and prints
void logd_main(void) {
    uint32_t mypid = process_get_current_pid();
    event_subscribe(mypid);
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_message(mypid, &msg) == 0) {
            // Null-terminate and print payload
            char buf[128];
            uint32_t n = (msg.size < sizeof(buf)-1) ? msg.size : sizeof(buf)-1;
            for (uint32_t i = 0; i < n; i++) buf[i] = (char)msg.data[i];
            buf[n] = '\0';
            log_info(buf);
        }
        asm volatile ("hlt");
    }
}

// fsd, netd, aidad placeholders
void fsd_main(void) { while (1) { asm volatile ("hlt"); } }
void netd_main(void) { while (1) { asm volatile ("hlt"); } }
void aidad_main(void) { while (1) { asm volatile ("hlt"); } }

// Simple kernel-space gfxd: draws a wallpaper gradient and a few icons
void gfxd_main(void) {
    framebuffer_info_t info;
    framebuffer_get_info(&info);
    uint32_t width = info.width;
    uint32_t height = info.height;
    uint8_t *back = framebuffer_get_back();
    if (!back) {
        log_warning("gfxd: no back buffer");
        while (1) asm volatile ("hlt");
    }
    // Gradient background
    for (uint32_t y = 0; y < height; y++) {
        uint8_t shade = (uint8_t)((y * 255) / (height-1));
        for (uint32_t x = 0; x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = (uint8_t)(shade / 3); // R
            back[off+1] = (uint8_t)(shade / 2); // G
            back[off+2] = shade;                // B
            back[off+3] = 0xFF;
        }
    }
    // Draw three icon boxes on desktop
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t ix = 40 + i*80;
        uint32_t iy = 40;
        for (uint32_t y = iy; y < iy+56 && y < height; y++) {
            for (uint32_t x = ix; x < ix+56 && x < width; x++) {
                uint32_t off = y * info.pitch + x * (info.bpp/8);
                back[off+0] = (uint8_t)(0x30 * (i+1));
                back[off+1] = (uint8_t)(0x80);
                back[off+2] = (uint8_t)(0xFF - 0x20*i);
                back[off+3] = 0xFF;
            }
        }
    }
    framebuffer_swap();
    while (1) asm volatile ("hlt");
}

// displayd: draws the taskbar, start button and start menu (static)
void displayd_main(void) {
    framebuffer_info_t info;
    framebuffer_get_info(&info);
    uint32_t width = info.width;
    uint32_t height = info.height;
    uint8_t *back = framebuffer_get_back();
    if (!back) {
        log_warning("displayd: no back buffer");
        while (1) asm volatile ("hlt");
    }
    // Taskbar
    uint32_t tb_h = 36;
    for (uint32_t y = height - tb_h; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0x22;
            back[off+1] = 0x22;
            back[off+2] = 0x22;
            back[off+3] = 0xFF;
        }
    }
    // Start button
    for (uint32_t y = height - tb_h + 4; y < height - 4; y++) {
        for (uint32_t x = 8; x < 92; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0x00;
            back[off+1] = 0x80;
            back[off+2] = 0x00;
            back[off+3] = 0xFF;
        }
    }
    // Start menu (static open)
    uint32_t sm_w = 220; uint32_t sm_h = 200;
    for (uint32_t y = height - tb_h - sm_h; y < height - tb_h; y++) {
        for (uint32_t x = 8; x < 8 + sm_w; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0x11;
            back[off+1] = 0x11;
            back[off+2] = 0x11;
            back[off+3] = 0xFF;
        }
    }
    // Menu items as stripes
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t item_y0 = height - tb_h - sm_h + 8 + i*28;
        for (uint32_t y = item_y0; y < item_y0+20; y++) {
            for (uint32_t x = 16; x < 8 + sm_w - 16; x++) {
                uint32_t off = y * info.pitch + x * (info.bpp/8);
                back[off+0] = (uint8_t)(0x40 + i*10);
                back[off+1] = (uint8_t)(0x40 + i*10);
                back[off+2] = (uint8_t)(0x40 + i*10);
                back[off+3] = 0xFF;
            }
        }
    }
    // Draw cursor
    uint32_t cx = (uint32_t)gui_cursor_x;
    uint32_t cy = (uint32_t)gui_cursor_y;
    if (cx < width && cy < height) {
        for (uint32_t y = cy; y < cy + 6 && y < height; y++) {
            for (uint32_t x = cx; x < cx + 6 && x < width; x++) {
                uint32_t off = y * info.pitch + x * (info.bpp/8);
                back[off+0] = 0xFF; back[off+1] = 0xFF; back[off+2] = 0xFF; back[off+3] = 0xFF;
            }
        }
    }
    framebuffer_swap();
    while (1) asm volatile ("hlt");
}


// hello_gui: draws a window-like rectangle with title bar
void hello_gui_main(void) {
    framebuffer_info_t info;
    framebuffer_get_info(&info);
    uint32_t width = info.width;
    uint32_t height = info.height;
    uint8_t *back = framebuffer_get_back();
    if (!back) {
        log_warning("hello_gui: no back buffer");
        while (1) asm volatile ("hlt");
    }
    uint32_t wx = 200, wy = 80, ww = 360, wh = 200;
    // Window body
    for (uint32_t y = wy; y < wy+wh && y < height; y++) {
        for (uint32_t x = wx; x < wx+ww && x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0xEE;
            back[off+1] = 0xEE;
            back[off+2] = 0xEE;
            back[off+3] = 0xFF;
        }
    }
    // Title bar
    for (uint32_t y = wy; y < wy+24 && y < height; y++) {
        for (uint32_t x = wx; x < wx+ww && x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0x40;
            back[off+1] = 0x60;
            back[off+2] = 0x90;
            back[off+3] = 0xFF;
        }
    }
    // Close button
    for (uint32_t y = wy+4; y < wy+20 && y < height; y++) {
        for (uint32_t x = wx+ww-28; x < wx+ww-8 && x < width; x++) {
            uint32_t off = y * info.pitch + x * (info.bpp/8);
            back[off+0] = 0xC0;
            back[off+1] = 0x40;
            back[off+2] = 0x40;
            back[off+3] = 0xFF;
        }
    }
    framebuffer_swap();
    while (1) asm volatile ("hlt");
}
