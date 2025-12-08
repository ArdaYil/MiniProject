#include <stdint.h>

/* Hardware Addresses */
volatile int* VGA_CTRL = (volatile int*)0x04000100;
volatile char* VGA = (volatile char*)0x08000000;
volatile int* BUTTONS = (volatile int*)0x040000d0;
volatile int* SWITCHES = (volatile int*)0x04000010;

/* Timer Addresses */
volatile int* TIMER_STATUS = (volatile int*)0x04000020;
volatile int* TIMER_CONTROL = (volatile int*)0x04000024;
volatile int* TIMER_LOW = (volatile int*)0x04000028;
volatile int* TIMER_HIGH = (volatile int*)0x0400002C;

/* External Functions */
extern void print(const char*);
extern void print_dec(unsigned int);

/* Performance Counter Helpers */
unsigned int get_mcycle() {
    unsigned int val;
    asm volatile("csrr %0, mcycle" : "=r"(val));
    return val;
}
unsigned int get_minstret() {
    unsigned int val;
    asm volatile("csrr %0, minstret" : "=r"(val));
    return val;
}
unsigned int get_hpm3() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter3" : "=r"(val));
    return val;
}
unsigned int get_hpm4() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter4" : "=r"(val));
    return val;
}
unsigned int get_hpm5() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter5" : "=r"(val));
    return val;
}
unsigned int get_hpm6() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter6" : "=r"(val));
    return val;
}
unsigned int get_hpm7() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter7" : "=r"(val));
    return val;
}
unsigned int get_hpm8() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter8" : "=r"(val));
    return val;
}
unsigned int get_hpm9() {
    unsigned int val;
    asm volatile("csrr %0, mhpmcounter9" : "=r"(val));
    return val;
}

/* Constants */
#define SCREEN_W 320
#define SCREEN_H 240
#define FLOOR_Y 200
#define GRAVITY 1
#define JUMP_FORCE -12

#define COLOR_BG 0x00
#define COLOR_CUBE 0xE0
#define COLOR_WAIT 0xFF
#define COLOR_SPIKE 0x1C
#define COLOR_FLOOR 0xFF
#define COLOR_DEATH 0x03
#define COLOR_WIN 0x1C
#define COLOR_GOAL 0x3F
#define COLOR_WALL 0xE6
#define COLOR_TRAP 0x30
#define COLOR_PLAT 0xFE

/* LEVEL DATA */
#define MAX_LEVELS 5
#define MAX_OBJS 80

/* Obstacle Types */
#define TYPE_SPIKE 0
#define TYPE_WALL 1
#define TYPE_TRAP 2
#define TYPE_PLAT 3

typedef struct {
    int x;
    int y;
    int type;
} LevelObj;

void handle_interrupt(unsigned _irq) {}

/* Level Data */
LevelObj ALL_LEVELS_DATA[MAX_LEVELS][MAX_OBJS] = {
    /* Level 1: Basic Spikes */
    {{600, 200, 0}, {1000, 200, 0}, {1400, 200, 0}, {2000, 200, 0}, {2500, 200, 0}, {-1, 0, 0}},
    /* Level 2: Faster Spikes */
    {{500, 200, 0}, {900, 200, 0}, {1200, 200, 0}, {1600, 200, 0}, {2000, 200, 0}, {2500, 200, 0}, {3000, 200, 0}, {-1, 0, 0}},
    /* Level 3: Walls */
    {{500, 200, 0}, {900, 180, 1}, {1200, 200, 0}, {1250, 200, 0}, {1600, 180, 1}, {2000, 180, 1}, {2400, 200, 0}, {3000, 180, 1}, {3500, 200, 0}, {-1, 0, 0}},
    /* Level 4: Platforms */
    {{500, 200, 0}, {800, 180, 1}, {1200, 170, 3}, {1300, 140, 3}, {1300, 200, 0}, {1400, 110, 3}, {1400, 200, 0}, {1600, 130, 2}, {1800, 110, 3}, {1800, 200, 0}, {1900, 140, 3}, {1900, 200, 0}, {2000, 170, 3}, {2500, 200, 0}, {3000, 180, 1}, {3500, 200, 0}, {-1, 0, 0}},
    /* Level 5: Faster platforms */
    {{500, 200, 0}, {700, 130, 2}, {1000, 180, 1}, {1300, 160, 3}, {1300, 200, 0}, {1360, 160, 3}, {1360, 200, 0}, {1500, 200, 0}, {1800, 140, 3}, {1850, 140, 3}, {1900, 130, 2}, {2200, 200, 0}, {2500, 120, 3}, {2500, 200, 0}, {2800, 200, 0}, {3100, 150, 3}, {3150, 120, 3}, {3150, 200, 0}, {3200, 90, 3}, {3200, 200, 0}, {3600, 200, 0}, {3650, 200, 0}, {4000, 180, 1}, {-1, 0, 0}}};

int LEVEL_GOALS[MAX_LEVELS] = {3000, 3500, 4000, 4500, 5000};
int LEVEL_SPEEDS[MAX_LEVELS] = {4, 5, 6, 7, 8};

int back_buffer_index = 0;

/* Graphics Functions */
void wait_for_vsync() {
    *(VGA_CTRL + 0) = 1;
    while (*(VGA_CTRL + 3) & 1);
}

void plot_pixel(int x, int y, char color) {
    unsigned int offset;
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    offset = back_buffer_index ? (SCREEN_W * SCREEN_H) : 0;
    *(VGA + offset + (y * SCREEN_W + x)) = color;
}

void clear_screen(char color) {
    int i;
    unsigned int offset = back_buffer_index ? (SCREEN_W * SCREEN_H) : 0;
    for (i = 0; i < SCREEN_W * SCREEN_H; i++) {
        *(VGA + offset + i) = color;
    }
}

void draw_rect(int x, int y, int w, int h, char color) {
    int i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            plot_pixel(x + j, y + i, color);
        }
    }
}

void draw_triangle(int x, int y, int height, char color) {
    int row, col;
    int half_width = height / 2;
    for (row = 0; row < height; row++) {
        int current_y = y - row;
        int current_half_width = half_width - (row / 2);
        if (current_half_width < 0) break;
        for (col = -current_half_width; col <= current_half_width; col++) {
            plot_pixel(x + col, current_y, color);
        }
    }
}

void draw_digit(int num, int x, int y, int w, int h, int t, char color) {
    if (num == 0) {
        /* Draw GO (G left of center, O right of center) */
        /* G */
        draw_rect(x - w, y, w, t, color);                 /* Top */
        draw_rect(x - w, y, t, h, color);                 /* Left */
        draw_rect(x - w, y + h - t, w, t, color);         /* Bot */
        draw_rect(x - t, y + h / 2, t, h / 2, color);     /* Bot-Right */
        draw_rect(x - w / 2, y + h / 2, w / 2, t, color); /* Inward */
        /* O */
        draw_rect(x + w / 2, y, w, t, color);         /* Top */
        draw_rect(x + w / 2, y + h - t, w, t, color); /* Bot */
        draw_rect(x + w / 2, y, t, h, color);         /* Left */
        draw_rect(x + w / 2 + w - t, y, t, h, color); /* Right */
    } else if (num == 1) {
        draw_rect(x + w / 2 - t / 2, y, t, h, color);
    } else if (num == 2) {
        draw_rect(x, y, w, t, color);             /* Top */
        draw_rect(x + w - t, y, t, h / 2, color); /* Top-Right */
        draw_rect(x, y + h / 2, w, t, color);     /* Mid */
        draw_rect(x, y + h / 2, t, h / 2, color); /* Bot-Left */
        draw_rect(x, y + h - t, w, t, color);     /* Bot */
    } else if (num == 3) {
        draw_rect(x, y, w, t, color);         /* Top */
        draw_rect(x + w - t, y, t, h, color); /* Right Side */
        draw_rect(x, y + h / 2, w, t, color); /* Mid */
        draw_rect(x, y + h - t, w, t, color); /* Bot */
    } else if (num == 4) {
        draw_rect(x, y, t, h / 2, color);     /* Top-Left */
        draw_rect(x, y + h / 2, w, t, color); /* Mid */
        draw_rect(x + w - t, y, t, h, color); /* Right */
    } else if (num == 5) {
        draw_rect(x, y, w, t, color);                     /* Top */
        draw_rect(x, y, t, h / 2, color);                 /* Top-Left */
        draw_rect(x, y + h / 2, w, t, color);             /* Mid */
        draw_rect(x + w - t, y + h / 2, t, h / 2, color); /* Bot-Right */
        draw_rect(x, y + h - t, w, t, color);             /* Bot */
    }
}

void draw_level_indicator(int current_level) {
    int i;
    int start_x = 125;
    int y = 10;
    int size = 10;
    for (i = 0; i < 5; i++) {
        int x = start_x + (i * 15);
        char color = (i == current_level) ? COLOR_GOAL : 0x01;
        draw_rect(x, y, size, size, color);
        if (i == current_level) {
            draw_rect(x + 3, y + 3, 4, 4, 0xFF);
        }
    }
}

int check_collision_tri(int box_x, int box_y, int box_s, int tri_x, int tri_y, int tri_h) {
    int box_left = box_x;
    int box_right = box_x + box_s;
    int box_top = box_y;
    int box_bottom = box_y + box_s;

    int tri_width = tri_h;
    int tri_left = tri_x - (tri_width / 2);
    int tri_right = tri_x + (tri_width / 2);
    int tri_top = tri_y - tri_h;
    int tri_bottom = tri_y;

    int forgiveness = 4;
    tri_left += forgiveness;
    tri_right -= forgiveness;
    tri_top += forgiveness;

    if (box_right < tri_left) return 0;
    if (box_left > tri_right) return 0;
    if (box_bottom < tri_top) return 0;
    if (box_top > tri_bottom) return 0;

    return 1;
}

int check_collision_box(int b1_x, int b1_y, int b1_s, int b2_x, int b2_y, int b2_w, int b2_h) {
    int b1_right = b1_x + b1_s;
    int b1_bottom = b1_y + b1_s;
    int b2_right = b2_x + b2_w;
    int b2_bottom = b2_y + b2_h;

    if (b1_right < b2_x) return 0;
    if (b1_x > b2_right) return 0;
    if (b1_bottom < b2_y) return 0;
    if (b1_y > b2_bottom) return 0;

    return 1;
}

void draw_menu_ui(int active_switch) {
    int i;
    clear_screen(COLOR_BG);
    for (i = 0; i < 5; i++) {
        int x = 40 + (i * 50);
        int y = 100;
        int size = 30;
        char color = (active_switch == i) ? COLOR_WIN : COLOR_CUBE;
        draw_rect(x, y, size, size, color);
        /* Use unified draw_digit for small numbers (w=12, h=20, t=3) */
        draw_digit(i + 1, x + 9, y + 5, 12, 20, 3, 0xFF);
    }
}

void timer_wait_one_second() {
    *TIMER_CONTROL = 0;
    *TIMER_STATUS = 0;
    *TIMER_LOW = 0xC380;
    *TIMER_HIGH = 0x01C9;
    *TIMER_CONTROL = 0x4;
    while ((*TIMER_STATUS & 1) == 0);
    *TIMER_STATUS = 0;
}

int main() {
    print("\n\n=== GEOMETRY DASH PERFORMANCE MONITOR ===\n");
    print("Toggle SW9 UP to enable detailed stats.\n");

    /* 0. OUTER APPLICATION LOOP */
    while (1) {
        int selected_level = -1;

        /* 1. Menu */
        while (selected_level == -1) {
            int switch_val = *SWITCHES;
            int current_selection = -1;
            int k;

            if (switch_val & 0x01) current_selection = 0;
            if (switch_val & 0x02) current_selection = 1;
            if (switch_val & 0x04) current_selection = 2;
            if (switch_val & 0x08) current_selection = 3;
            if (switch_val & 0x10) current_selection = 4;

            draw_menu_ui(current_selection);
            wait_for_vsync();

            back_buffer_index = !back_buffer_index;
            *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);

            if (current_selection != -1) {
                selected_level = current_selection;
                for (k = 0; k < 10; k++) wait_for_vsync();
            }
        }

        /* 2. Countdown */
        {
            int count;
            for (count = 4; count >= 0; count--) {
                clear_screen(COLOR_BG);
                draw_level_indicator(selected_level);
                {
                    int x;
                    for (x = 0; x < SCREEN_W; x++) plot_pixel(x, FLOOR_Y, COLOR_FLOOR);
                }

                /* Pass Green for 'GO' (0), else Red */
                {
                    char c = (count == 0) ? COLOR_WIN : COLOR_CUBE;
                    /* Use unified draw_digit for big numbers (w=40, h=60, t=8) */
                    draw_digit(count, 140, 90, 40, 60, 8, c);
                }

                wait_for_vsync();
                back_buffer_index = !back_buffer_index;
                *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);
                timer_wait_one_second();
            }
        }

        /* 3. Init Game */
        {
            LevelObj* current_level_data = ALL_LEVELS_DATA[selected_level];
            int current_goal_x = LEVEL_GOALS[selected_level];
            int current_speed = LEVEL_SPEEDS[selected_level];

            int cube_x = 50;
            int cube_y = FLOOR_Y - 20;
            int cube_dy = 0;
            int cube_size = 20;
            int is_grounded = 1;

            int scroll_x = 0;
            int game_over = 0;
            int level_complete = 0;

            /* Performance Accumulators */
            unsigned int total_cycles = 0;
            unsigned int total_inst = 0;
            unsigned int total_mem = 0;
            unsigned int total_imiss = 0;
            unsigned int total_dmiss = 0;
            unsigned int total_istall = 0;
            unsigned int total_dstall = 0;
            unsigned int total_haz = 0;
            unsigned int total_alu = 0;

            /* Temporary counters */
            unsigned int s_cyc, s_inst, s_h3, s_h4, s_h5, s_h6, s_h7, s_h8, s_h9;
            unsigned int e_cyc, e_inst, e_h3, e_h4, e_h5, e_h6, e_h7, e_h8, e_h9;

            /* 4. Game Loop */
            while (1) {
                if (!game_over && !level_complete) {
                    int button_pressed;
                    int i;
                    int benchmark_mode = (*SWITCHES & 0x200);

                    /* PERFORMANCE START */
                    if (benchmark_mode) {
                        s_cyc = get_mcycle();
                        s_inst = get_minstret();
                        s_h3 = get_hpm3();
                        s_h4 = get_hpm4();
                        s_h5 = get_hpm5();
                        s_h6 = get_hpm6();
                        s_h7 = get_hpm7();
                        s_h8 = get_hpm8();
                        s_h9 = get_hpm9();
                    }

                    /* LOGIC START */
                    button_pressed = (*BUTTONS & 1);
                    if (button_pressed && is_grounded) {
                        cube_dy = JUMP_FORCE;
                        is_grounded = 0;
                    }
                    cube_y += cube_dy;
                    cube_dy += GRAVITY;
                    if (cube_y > FLOOR_Y - cube_size) {
                        cube_y = FLOOR_Y - cube_size;
                        cube_dy = 0;
                        is_grounded = 1;
                    }
                    scroll_x += current_speed;

                    for (i = 0; i < MAX_OBJS; i++) {
                        int obj_x = current_level_data[i].x;
                        int obj_y = current_level_data[i].y;
                        int type = current_level_data[i].type;
                        int screen_x;
                        if (obj_x == -1) break;
                        screen_x = obj_x - scroll_x;

                        if (screen_x > -50 && screen_x < 350) {
                            int hit = 0;
                            int landed = 0;
                            if (type == TYPE_SPIKE)
                                hit = check_collision_tri(cube_x, cube_y, cube_size, screen_x, obj_y, 20);
                            else if (type == TYPE_WALL)
                                hit = check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, 20, 20);
                            else if (type == TYPE_TRAP)
                                hit = check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, 20, 20);
                            else if (type == TYPE_PLAT) {
                                if (check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, 40, 20)) {
                                    if (cube_dy >= 0 && (cube_y + cube_size) <= (obj_y + 12))
                                        landed = 1;
                                    else
                                        hit = 1;
                                }
                            }
                            if (landed) {
                                cube_y = obj_y - cube_size;
                                cube_dy = 0;
                                is_grounded = 1;
                            }
                            if (hit) game_over = 1;
                        }
                    }
                    if ((current_goal_x - scroll_x) <= cube_x) level_complete = 1;
                    /* LOGIC END */

                    /* PERFORMANCE END */
                    if (benchmark_mode) {
                        e_cyc = get_mcycle();
                        e_inst = get_minstret();
                        e_h3 = get_hpm3();
                        e_h4 = get_hpm4();
                        e_h5 = get_hpm5();
                        e_h6 = get_hpm6();
                        e_h7 = get_hpm7();
                        e_h8 = get_hpm8();
                        e_h9 = get_hpm9();

                        /* Accumulate */
                        total_cycles += (e_cyc - s_cyc);
                        total_inst += (e_inst - s_inst);
                        total_mem += (e_h3 - s_h3);
                        total_imiss += (e_h4 - s_h4);
                        total_dmiss += (e_h5 - s_h5);
                        total_istall += (e_h6 - s_h6);
                        total_dstall += (e_h7 - s_h7);
                        total_haz += (e_h8 - s_h8);
                        total_alu += (e_h9 - s_h9);
                    }
                }

                /* Draw */
                if (game_over)
                    clear_screen(COLOR_DEATH);
                else if (level_complete)
                    clear_screen(COLOR_WIN);
                else
                    clear_screen(COLOR_BG);

                if (!game_over && !level_complete) {
                    int x, i;
                    int goal_screen_x;
                    draw_level_indicator(selected_level);
                    for (x = 0; x < SCREEN_W; x++) plot_pixel(x, FLOOR_Y, COLOR_FLOOR);
                    for (i = 0; i < MAX_OBJS; i++) {
                        int obj_x = current_level_data[i].x;
                        int obj_y = current_level_data[i].y;
                        int type = current_level_data[i].type;
                        int screen_x;
                        if (obj_x == -1) break;
                        screen_x = obj_x - scroll_x;
                        if (screen_x > -20 && screen_x < 320) {
                            if (type == TYPE_SPIKE)
                                draw_triangle(screen_x, obj_y, 20, COLOR_SPIKE);
                            else if (type == TYPE_WALL)
                                draw_rect(screen_x, obj_y, 20, 20, COLOR_WALL);
                            else if (type == TYPE_TRAP)
                                draw_rect(screen_x, obj_y, 20, 20, COLOR_TRAP);
                            else if (type == TYPE_PLAT)
                                draw_rect(screen_x, obj_y, 40, 20, COLOR_PLAT);
                        }
                    }
                    goal_screen_x = current_goal_x - scroll_x;
                    if (goal_screen_x > -20 && goal_screen_x < 320) draw_rect(goal_screen_x, FLOOR_Y - 200, 20, 200, COLOR_WIN);
                    draw_rect(cube_x, cube_y, cube_size, cube_size, COLOR_CUBE);
                }

                wait_for_vsync();
                back_buffer_index = !back_buffer_index;
                *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);

                /* END OF GAME LOGIC & REPORTING */
                if (game_over || level_complete) {
                    print("\n\n=== LEVEL FINISHED ===\n");
                    if (level_complete)
                        print("RESULT: WIN\n");
                    else
                        print("RESULT: GAME OVER\n");
                    print("Total Cycles: ");
                    print_dec(total_cycles);
                    print("\n");
                    print("Total Instr : ");
                    print_dec(total_inst);
                    print("\n");
                    print("Mem Instr   : ");
                    print_dec(total_mem);
                    print("\n");
                    print("I-Cache Miss: ");
                    print_dec(total_imiss);
                    print("\n");
                    print("D-Cache Miss: ");
                    print_dec(total_dmiss);
                    print("\n");
                    print("I-Cache Stal: ");
                    print_dec(total_istall);
                    print("\n");
                    print("D-Cache Stal: ");
                    print_dec(total_dstall);
                    print("\n");
                    print("Hazard Stall: ");
                    print_dec(total_haz);
                    print("\n");
                    print("ALU Stall   : ");
                    print_dec(total_alu);
                    print("\n");
                    print("======================\n");

                    while (!(*BUTTONS & 1));
                    while ((*BUTTONS & 1));
                    break;
                }
            }
        }
    }
}