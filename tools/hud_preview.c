#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define WIDTH  640
#define HEIGHT 480

static uint8_t buf[HEIGHT][WIDTH][3];

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    buf[y][x][0] = r; buf[y][x][1] = g; buf[y][x][2] = b;
}

static void set_bright(int x, int y) { set_pixel(x, y, 0, 255, 51); }
static void set_dim(int x, int y)    { set_pixel(x, y, 0,  89, 20); }

static const uint32_t DIGITS[10] = {
    0x699996u, 0x262227u, 0x69124Fu, 0x691316u, 0x99F111u,
    0xF8E196u, 0x68F996u, 0xF12444u, 0x696996u, 0x699716u,
};

static const uint32_t LETTERS[14] = {
    0xE9999Eu,
    0xF8E88Fu,
    0xF8E888u,
    0x99F999u,
    0xF6666Fu,
    0x9F9999u,
    0x9DB999u,
    0x699996u,
    0xE9ECA9u,
    0x78611Eu,
    0xF44444u,
    0x999996u,
    0x996699u,
    0x842480u,
};

enum { LD=0, LE=1, LF=2, LH=3, LI=4, LM=5, LN=6, LO=7, LR=8, LS=9, LT=10, LU=11, LX=12, LCAR=13 };

static int digit_px(int d, int col, int row) {
    if (d < 0 || d > 9 || col < 0 || col > 3 || row < 0 || row > 5) return 0;
    return (int)((DIGITS[d] >> (unsigned)((5-row)*4+(3-col))) & 1u);
}

static int letter_px(int idx, int col, int row) {
    if (idx < 0 || idx > 13 || col < 0 || col > 3 || row < 0 || row > 5) return 0;
    return (int)((LETTERS[idx] >> (unsigned)((5-row)*4+(3-col))) & 1u);
}

static void draw_digit(int d, int ox, int oy) {
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 4; c++)
            if (digit_px(d, c, r))
                set_bright(ox + c, oy + r);
}

static void draw_letter(int idx, int ox, int oy) {
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 4; c++)
            if (letter_px(idx, c, r))
                set_bright(ox + c, oy + r);
}

static void draw_line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1-x0), dy = abs(y1-y0);
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1, err = dx-dy;
    while (1) {
        set_bright(x0, y0);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

static void darken(void) {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) {
            buf[y][x][0] = (uint8_t)(buf[y][x][0] * 22 / 100);
            buf[y][x][1] = (uint8_t)(buf[y][x][1] * 22 / 100);
            buf[y][x][2] = (uint8_t)(buf[y][x][2] * 22 / 100);
        }
}

static void draw_settings(int settings_cursor, int mode, int ir_res_preset, int hud_enabled) {
    darken();

    int w = WIDTH, h = HEIGHT;
    int cx = w/2, cy = h/2;
    int pl = cx-150, pr = cx+150, pt = cy-100, pb = cy+100;

    for (int x = pl; x <= pr; x++) {
        set_bright(x, pt); set_bright(x, pb);
    }
    for (int y = pt+1; y < pb; y++) {
        set_bright(pl, y); set_bright(pr, y);
    }

    for (int x = pl+1; x <= pr-1; x++)
        set_bright(x, pt+18);

    int rows[4] = { pt+30, pt+62, pt+94, pt+126 };

    for (int i = 0; i < 4; i++) {
        int iy = rows[i];

        if (i == settings_cursor)
            draw_letter(LCAR, pl+8, iy);

        int lx = pl+24;
        if (i == 0) {
            draw_letter(LM,  lx,    iy);
            draw_letter(LO,  lx+5,  iy);
            draw_letter(LD,  lx+10, iy);
            draw_letter(LE,  lx+15, iy);
        }
        if (i == 1) {
            draw_letter(LR,  lx,    iy);
            draw_letter(LE,  lx+5,  iy);
            draw_letter(LS,  lx+10, iy);
        }
        if (i == 2) {
            draw_letter(LH,  lx,    iy);
            draw_letter(LU,  lx+5,  iy);
            draw_letter(LD,  lx+10, iy);
        }
        if (i == 3) {
            draw_letter(LE,  lx,    iy);
            draw_letter(LX,  lx+5,  iy);
            draw_letter(LI,  lx+10, iy);
            draw_letter(LT,  lx+15, iy);
        }

        int vx = pl+155;
        if (i == 0) {
            draw_digit(mode, vx, iy);
        }
        if (i == 1) {
            for (int p = 0; p < 3; p++) {
                int bx0 = vx + p*16, bx1 = bx0 + 12;
                for (int x = bx0; x <= bx1; x++) {
                    set_bright(x, iy); set_bright(x, iy+6);
                }
                for (int y = iy+1; y < iy+6; y++) {
                    set_bright(bx0, y); set_bright(bx1, y);
                }
                if (p == ir_res_preset) {
                    for (int y = iy+1; y < iy+6; y++)
                        for (int x = bx0+1; x < bx1; x++)
                            set_bright(x, y);
                }
            }
        }
        if (i == 2) {
            if (hud_enabled) {
                draw_letter(LO, vx,   iy);
                draw_letter(LN, vx+5, iy);
            } else {
                draw_letter(LO, vx,    iy);
                draw_letter(LF, vx+5,  iy);
                draw_letter(LF, vx+10, iy);
            }
        }
    }
}

static void draw_hud(float ir_intensity, int has_temp, int ir_on,
                     int crosshair_on, float crosshair_dist,
                     int settings_open, int settings_cursor,
                     int mode, int ir_res_preset, int hud_enabled) {
    int w = WIDTH, h = HEIGHT;
    int cx = w/2, cy = h/2;

    if (settings_open) {
        draw_settings(settings_cursor, mode, ir_res_preset, hud_enabled);
        return;
    }

    if (!hud_enabled)
        return;

    int bw = 130, bh = 18, bgap = 6;
    int bx0 = 10, bx1 = bx0 + bw;
    int panel_h  = 4*bh + 3*bgap;
    int by_end   = h - 10;
    int by_start = by_end - panel_h;

    int stat[4] = { 1, ir_on ? 1 : 0, has_temp ? 1 : 0, 1 };

    for (int i = 0; i < 4; i++) {
        int y0 = by_start + i*(bh+bgap);
        int y1 = y0 + bh - 1;
        void (*p)(int,int) = stat[i] ? set_bright : set_dim;
        for (int x = bx0; x <= bx1; x++) { p(x, y0); p(x, y1); }
        for (int y = y0+1; y < y1; y++)   { p(bx0, y); p(bx1, y); }
    }

    if (crosshair_on) {
        int arm = 12;
        draw_line(cx, cy, cx - arm, cy + arm);
        draw_line(cx, cy, cx + arm, cy + arm);
    }

    if (crosshair_on) {
        int dist_m = crosshair_dist > 0.0f ? (int)(crosshair_dist + 0.5f) : 0;
        int tx = w - 10 - 19;
        int ty = h - 10 - 6;
        draw_digit((dist_m/100)%10, tx,    ty);
        draw_digit((dist_m/ 10)%10, tx+5,  ty);
        draw_digit( dist_m     %10, tx+10, ty);
        set_bright(tx+15, ty);   set_bright(tx+19, ty);
        set_bright(tx+16, ty+1); set_bright(tx+18, ty+1);
        set_bright(tx+15, ty+2); set_bright(tx+17, ty+2); set_bright(tx+19, ty+2);
        set_bright(tx+15, ty+3); set_bright(tx+19, ty+3);
        set_bright(tx+15, ty+4); set_bright(tx+19, ty+4);
        set_bright(tx+15, ty+5); set_bright(tx+19, ty+5);
    }

    int br_x0 = w - 24, br_x1 = w - 14;
    int br_y0 = h/3,    br_y1 = 2*h/3;
    float fc  = ir_intensity < 0.0f ? 0.0f : ir_intensity > 1.0f ? 1.0f : ir_intensity;
    int fill_y = br_y1 - (int)((br_y1 - br_y0) * fc);

    for (int x = br_x0; x <= br_x1; x++) {
        for (int y = br_y0; y <= br_y1; y++) {
            int edge = (x==br_x0 || x==br_x1 || y==br_y0 || y==br_y1);
            if (edge || y >= fill_y) set_bright(x, y);
        }
    }
}

static void write_bmp(const char *path) {
    int stride = WIDTH * 3;
    int pad    = (4 - (stride % 4)) % 4;
    int rowsz  = stride + pad;
    int datasz = rowsz * HEIGHT;
    int filesz = 14 + 40 + datasz;

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }

    uint8_t fh[14] = {0};
    fh[0]='B'; fh[1]='M';
    fh[2]=filesz&0xFF; fh[3]=(filesz>>8)&0xFF;
    fh[4]=(filesz>>16)&0xFF; fh[5]=(filesz>>24)&0xFF;
    fh[10]=54;
    fwrite(fh, 14, 1, f);

    uint8_t dib[40] = {0};
    dib[0]=40;
    dib[4]=WIDTH &0xFF; dib[5]=(WIDTH >>8)&0xFF;
    dib[6]=(WIDTH >>16)&0xFF; dib[7]=(WIDTH >>24)&0xFF;
    dib[8]=HEIGHT&0xFF; dib[9]=(HEIGHT>>8)&0xFF;
    dib[10]=(HEIGHT>>16)&0xFF; dib[11]=(HEIGHT>>24)&0xFF;
    dib[12]=1; dib[14]=24;
    dib[20]=datasz&0xFF; dib[21]=(datasz>>8)&0xFF;
    dib[22]=(datasz>>16)&0xFF; dib[23]=(datasz>>24)&0xFF;
    fwrite(dib, 40, 1, f);

    uint8_t row[WIDTH*3+4];
    for (int y = HEIGHT-1; y >= 0; y--) {
        memset(row, 0, sizeof(row));
        for (int x = 0; x < WIDTH; x++) {
            row[x*3+0] = buf[y][x][2];
            row[x*3+1] = buf[y][x][1];
            row[x*3+2] = buf[y][x][0];
        }
        fwrite(row, rowsz, 1, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    float ir_intensity   = 0.6f;
    int   has_temp       = 0;
    int   ir_on          = 1;
    int   crosshair_on   = 1;
    float crosshair_dist = 42.0f;
    int   settings_open  = 0;
    int   settings_cursor = 0;
    int   mode           = 1;
    int   ir_res_preset  = 1;
    int   hud_enabled    = 1;

    if (argc >= 2)  ir_intensity   = (float)atof(argv[1]);
    if (argc >= 3)  has_temp       = atoi(argv[2]);
    if (argc >= 4)  ir_on          = atoi(argv[3]);
    if (argc >= 5)  crosshair_on   = atoi(argv[4]);
    if (argc >= 6)  crosshair_dist = (float)atof(argv[5]);
    if (argc >= 7)  settings_open  = atoi(argv[6]);
    if (argc >= 8)  settings_cursor = atoi(argv[7]);
    if (argc >= 9)  mode           = atoi(argv[8]);
    if (argc >= 10) ir_res_preset  = atoi(argv[9]);
    if (argc >= 11) hud_enabled    = atoi(argv[10]);

    memset(buf, 10, sizeof(buf));
    draw_hud(ir_intensity, has_temp, ir_on, crosshair_on, crosshair_dist,
             settings_open, settings_cursor, mode, ir_res_preset, hud_enabled);
    write_bmp("hud_preview.bmp");
    return 0;
}