#include <stdint.h>
#include <stdbool.h>

// --- Hardware Addresses ---
volatile int *VGA_CTRL = (volatile int*) 0x04000100;
volatile char *VGA     = (volatile char*) 0x08000000;
volatile int *BUTTONS  = (volatile int*) 0x040000d0; 
volatile int *SWITCHES = (volatile int*) 0x04000010;

// --- Timer Addresses ---
volatile int *TIMER_STATUS  = (volatile int*) 0x04000020;
volatile int *TIMER_CONTROL = (volatile int*) 0x04000024;
volatile int *TIMER_LOW     = (volatile int*) 0x04000028;
volatile int *TIMER_HIGH    = (volatile int*) 0x0400002C;

// --- Constants ---
#define SCREEN_W 320
#define SCREEN_H 240
#define FLOOR_Y  200

// Physics
#define GRAVITY     1
#define JUMP_FORCE  -12

// Colors
#define COLOR_BG      0x00 // Black
#define COLOR_CUBE    0xE0 // Red
#define COLOR_WAIT    0xFF // Yellow
#define COLOR_SPIKE   0x1C // Green
#define COLOR_FLOOR   0xFF // White
#define COLOR_DEATH   0x03 // Blue 
#define COLOR_WIN     0x1C // Green 
#define COLOR_GOAL    0x3F // Cyan 
#define COLOR_WALL    0xE6 // Orange (Block)
#define COLOR_TRAP    0x30 // Magenta (Air Mine)
#define COLOR_PLAT    0xFE // Yellow (Platform)

// --- LEVEL DATA ---
#define MAX_LEVELS 5
#define MAX_OBJS   80 // Increased to fit extra spikes

// Obstacle Types
#define TYPE_SPIKE 0
#define TYPE_WALL  1
#define TYPE_TRAP  2
#define TYPE_PLAT  3 // Platform you can jump on

typedef struct {
    int x;
    int y;    // Y Position (Bottom for Spike, Top for others)
    int type;
} LevelObj;

void handle_interrupt (unsigned _irq) { }

// Level Data
// Format: { X_POSITION, Y_POSITION, TYPE }
// Spikes: Y=200 (Floor)
// Walls:  Y=180 (Top of block)
// Traps:  Y=130 (Air)
// Plats:  Y=Variable
LevelObj ALL_LEVELS_DATA[MAX_LEVELS][MAX_OBJS] = {
    // Level 1: Basic Spikes
    { {600,200,0}, {1000,200,0}, {1400,200,0}, {2000,200,0}, {2500,200,0}, {-1,0,0} }, 
    
    // Level 2: Faster Spikes
    { {500,200,0}, {900,200,0}, {1200,200,0}, {1600,200,0}, {2000,200,0}, {2500,200,0}, {3000,200,0}, {-1,0,0} },
    
    // Level 3: Walls
    { {500,200,0}, {900,180,1}, {1200,200,0}, {1250,200,0}, {1600,180,1}, {2000,180,1}, {2400,200,0}, {3000,180,1}, {3500,200,0}, {-1,0,0} },
    
    // Level 4: Stairs Introduction
    // Added Spikes (Type 0) under the high platforms to force climbing
    { 
      {500,200,0}, {800,180,1}, 
      {1200,170,3}, 
      {1300,140,3}, {1300,200,0}, // Spike under
      {1400,110,3}, {1400,200,0}, // Spike under
      {1600,130,2}, // Trap under the jump
      {1800,110,3}, {1800,200,0}, // Spike under
      {1900,140,3}, {1900,200,0}, // Spike under
      {2000,170,3}, 
      {2500,200,0}, {3000,180,1}, {3500,200,0}, 
      {-1,0,0} 
    },
    
    // Level 5: The Gauntlet (Platforms + Spikes)
    { 
      {500,200,0}, {700,130,2}, {1000,180,1}, 
      {1300,160,3}, {1300,200,0}, 
      {1360,160,3}, {1360,200,0}, 
      {1500,200,0}, 
      {1800,140,3}, {1850,140,3}, {1900,130,2}, 
      {2200,200,0}, 
      {2500,120,3}, {2500,200,0}, // High jump with spike
      {2800,200,0}, 
      {3100,150,3}, 
      {3150,120,3}, {3150,200,0}, 
      {3200,90,3},  {3200,200,0}, 
      {3600,200,0}, {3650,200,0}, {4000,180,1}, 
      {-1,0,0} 
    }
};

int LEVEL_GOALS[MAX_LEVELS] = { 3000, 3500, 4000, 4500, 5000 };
int LEVEL_SPEEDS[MAX_LEVELS] = { 4, 5, 6, 7, 8 };

int back_buffer_index = 0;

// --- Graphics Functions ---

void wait_for_vsync() {
    *(VGA_CTRL + 0) = 1;
    while (*(VGA_CTRL + 3) & 1);
}

void plot_pixel(int x, int y, char color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    unsigned int offset = back_buffer_index ? (SCREEN_W * SCREEN_H) : 0;
    *(VGA + offset + (y * SCREEN_W + x)) = color;
}

void clear_screen(char color) {
    unsigned int offset = back_buffer_index ? (SCREEN_W * SCREEN_H) : 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        *(VGA + offset + i) = color;
    }
}

void draw_rect(int x, int y, int w, int h, char color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            plot_pixel(x + j, y + i, color);
        }
    }
}

void draw_triangle(int x, int y, int height, char color) {
    int half_width = height / 2;
    for (int row = 0; row < height; row++) {
        int current_y = y - row;
        int current_half_width = half_width - (row / 2);
        if (current_half_width < 0) break;
        for (int col = -current_half_width; col <= current_half_width; col++) {
            plot_pixel(x + col, current_y, color);
        }
    }
}

void draw_level_indicator(int current_level) {
    int start_x = 125; 
    int y = 10;
    int size = 10;
    for (int i = 0; i < 5; i++) {
        int x = start_x + (i * 15); 
        char color = (i == current_level) ? COLOR_GOAL : 0x01; 
        draw_rect(x, y, size, size, color);
        if (i == current_level) {
            draw_rect(x + 3, y + 3, 4, 4, 0xFF);
        }
    }
}

// Check collision for Triangle (Spike)
bool check_collision_tri(int box_x, int box_y, int box_s, int tri_x, int tri_y, int tri_h) {
    int box_left   = box_x;
    int box_right  = box_x + box_s;
    int box_top    = box_y;
    int box_bottom = box_y + box_s;

    int tri_width = tri_h; 
    int tri_left   = tri_x - (tri_width / 2);
    int tri_right  = tri_x + (tri_width / 2);
    int tri_top    = tri_y - tri_h;
    int tri_bottom = tri_y;

    int forgiveness = 4;
    tri_left   += forgiveness;
    tri_right  -= forgiveness;
    tri_top    += forgiveness;

    if (box_right < tri_left) return false;
    if (box_left > tri_right) return false;
    if (box_bottom < tri_top) return false;
    if (box_top > tri_bottom) return false;

    return true;
}

// Check collision for Rectangle (Wall/Trap/Plat)
bool check_collision_box(int b1_x, int b1_y, int b1_s, int b2_x, int b2_y, int b2_w, int b2_h) {
    int b1_right  = b1_x + b1_s;
    int b1_bottom = b1_y + b1_s;
    int b2_right  = b2_x + b2_w;
    int b2_bottom = b2_y + b2_h;

    // Standard AABB
    if (b1_right < b2_x) return false;
    if (b1_x > b2_right) return false;
    if (b1_bottom < b2_y) return false;
    if (b1_y > b2_bottom) return false;

    return true;
}

// --- NEW: Helper for menu numbers (Small 1-5) ---
void draw_small_digit(int num, int x, int y, char color) {
    int w = 12; int h = 20; int t = 3;
    if (num == 1) {
        draw_rect(x + w/2, y, t, h, color);
    } else if (num == 2) {
        draw_rect(x, y, w, t, color);         // Top
        draw_rect(x+w-t, y, t, h/2, color);   // Top-Right
        draw_rect(x, y+h/2, w, t, color);     // Mid
        draw_rect(x, y+h/2, t, h/2, color);   // Bot-Left
        draw_rect(x, y+h-t, w, t, color);     // Bot
    } else if (num == 3) {
        draw_rect(x, y, w, t, color);         // Top
        draw_rect(x+w-t, y, t, h, color);     // Right side
        draw_rect(x, y+h/2, w, t, color);     // Mid
        draw_rect(x, y+h-t, w, t, color);     // Bot
    } else if (num == 4) {
        draw_rect(x, y, t, h/2, color);       // Top-Left
        draw_rect(x, y+h/2, w, t, color);     // Mid
        draw_rect(x+w-t, y, t, h, color);     // Right (Full)
    } else if (num == 5) {
        draw_rect(x, y, w, t, color);         // Top
        draw_rect(x, y, t, h/2, color);       // Top-Left
        draw_rect(x, y+h/2, w, t, color);     // Mid
        draw_rect(x+w-t, y+h/2, t, h/2, color); // Bot-Right
        draw_rect(x, y+h-t, w, t, color);     // Bot
    }
}

void draw_menu_ui(int active_switch) {
    clear_screen(COLOR_BG);
    // REMOVED: draw_rect(0, 0, 320, 40, 0x01); // Blue Header
    for(int i=0; i<5; i++) {
        int x = 40 + (i * 50);
        int y = 100;
        int size = 30;
        char color = (active_switch == i) ? COLOR_WIN : COLOR_CUBE; 
        draw_rect(x, y, size, size, color);
        
        // Draw Number inside the box (White)
        draw_small_digit(i + 1, x + 9, y + 5, 0xFF);
    }
    // REMOVED: draw_rect(0, 200, 320, 2, 0xFF); // Floor Line
}

void draw_big_digit(int num, int x, int y, char color) {
    int w = 40; int h = 60; int t = 8;
    if (num == 4) {
        draw_rect(x, y, t, h/2, color);       // Top-Left
        draw_rect(x, y+h/2, w, t, color);     // Middle
        draw_rect(x+w-t, y, t, h, color);     // Right (Full height)
    } else if (num == 3) {
        draw_rect(x, y, w, t, color);         
        draw_rect(x, y+h/2, w, t, color);     
        draw_rect(x, y+h-t, w, t, color);     
        draw_rect(x+w-t, y, t, h, color);     
    } else if (num == 2) {
        draw_rect(x, y, w, t, color);         
        draw_rect(x, y+h/2, w, t, color);     
        draw_rect(x, y+h-t, w, t, color);     
        draw_rect(x+w-t, y, t, h/2, color);   
        draw_rect(x, y+h/2, t, h/2, color);   
    } else if (num == 1) {
        draw_rect(x + w/2 - t/2, y, t, h, color); 
    } else if (num == 0) {
        draw_rect(x-40, y, w, t, COLOR_WIN);        
        draw_rect(x-40, y, t, h, COLOR_WIN);        
        draw_rect(x-40, y+h-t, w, t, COLOR_WIN);    
        draw_rect(x-40+w-t, y+h/2, t, h/2, COLOR_WIN); 
        draw_rect(x-40+w/2, y+h/2, w/2, t, COLOR_WIN); 
        draw_rect(x+20, y, w, t, COLOR_WIN);        
        draw_rect(x+20, y+h-t, w, t, COLOR_WIN);    
        draw_rect(x+20, y, t, h, COLOR_WIN);        
        draw_rect(x+20+w-t, y, t, h, COLOR_WIN);    
    }
}

void timer_wait_one_second() {
    *TIMER_CONTROL = 0;
    *TIMER_STATUS = 0;
    *TIMER_LOW  = 0xC380;
    *TIMER_HIGH = 0x01C9;
    *TIMER_CONTROL = 0x4; 
    while ((*TIMER_STATUS & 1) == 0);
    *TIMER_STATUS = 0;
}

int main() {
    // --- 0. OUTER APPLICATION LOOP ---
    // This allows the game to restart indefinitely
    while(1) {
        int selected_level = -1;
        
        // 1. Menu
        while (selected_level == -1) {
            int switch_val = *SWITCHES;
            int current_selection = -1;
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
                for(int k=0; k<10; k++) wait_for_vsync();
            }
        }

        // 2. Countdown (Now starts from 4)
        for (int count = 4; count >= 0; count--) {
            clear_screen(COLOR_BG);
            draw_level_indicator(selected_level);
            for (int x = 0; x < SCREEN_W; x++) plot_pixel(x, FLOOR_Y, COLOR_FLOOR);
            // All numbers are now RED (COLOR_CUBE)
            draw_big_digit(count, 140, 90, COLOR_CUBE);
            wait_for_vsync();
            back_buffer_index = !back_buffer_index;
            *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);
            timer_wait_one_second();
        }

        // 3. Init Game
        LevelObj *current_level_data = ALL_LEVELS_DATA[selected_level];
        int current_goal_x = LEVEL_GOALS[selected_level];
        int current_speed = LEVEL_SPEEDS[selected_level];

        int cube_x = 50; 
        int cube_y = FLOOR_Y - 20;
        int cube_dy = 0; 
        int cube_size = 20;
        bool is_grounded = true;

        int scroll_x = 0; 
        bool game_over = false;
        bool level_complete = false;

        // 4. Game Loop
        while (1) {
            if (!game_over && !level_complete) {
                
                // Logic
                int button_pressed = (*BUTTONS & 1); 
                if (button_pressed && is_grounded) {
                    cube_dy = JUMP_FORCE;
                    is_grounded = false;
                }
                cube_y += cube_dy; 
                cube_dy += GRAVITY; 

                // Floor Collision (Base Floor)
                if (cube_y > FLOOR_Y - cube_size) {
                    cube_y = FLOOR_Y - cube_size; 
                    cube_dy = 0;                  
                    is_grounded = true;           
                }

                scroll_x += current_speed;

                // Object Logic
                for (int i = 0; i < MAX_OBJS; i++) {
                    int obj_x = current_level_data[i].x;
                    int obj_y = current_level_data[i].y;
                    int type  = current_level_data[i].type;
                    
                    if (obj_x == -1) break; 
                    int screen_x = obj_x - scroll_x;
                    
                    // Only check if on screen
                    if (screen_x > -50 && screen_x < 350) {
                        bool hit = false;
                        bool landed = false;

                        if (type == TYPE_SPIKE) {
                            hit = check_collision_tri(cube_x, cube_y, cube_size, screen_x, obj_y, 20);
                        } else if (type == TYPE_WALL) {
                            hit = check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, 20, 20);
                        } else if (type == TYPE_TRAP) {
                            hit = check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, 20, 20);
                        } else if (type == TYPE_PLAT) {
                            // Platform Collision Check
                            int plat_w = 40; // Platforms are wider
                            int plat_h = 20;
                            if (check_collision_box(cube_x, cube_y, cube_size, screen_x, obj_y, plat_w, plat_h)) {
                                // Did we land on top?
                                // Condition: Falling AND Feet were previously above the platform top (approx)
                                if (cube_dy >= 0 && (cube_y + cube_size) <= (obj_y + 12)) {
                                    landed = true;
                                } else {
                                    hit = true; // Hit the side or bottom
                                }
                            }
                        }

                        if (landed) {
                            cube_y = obj_y - cube_size;
                            cube_dy = 0;
                            is_grounded = true;
                        }
                        
                        if (hit) game_over = true;
                    }
                }

                if ((current_goal_x - scroll_x) <= cube_x) level_complete = true; 
            } 

            // Draw
            if (game_over) {
                clear_screen(COLOR_DEATH); 
            } else if (level_complete) {
                clear_screen(COLOR_WIN);   
            } else {
                clear_screen(COLOR_BG);    
            }

            if (!game_over && !level_complete) {
                draw_level_indicator(selected_level);
                for (int x = 0; x < SCREEN_W; x++) plot_pixel(x, FLOOR_Y, COLOR_FLOOR);
                
                // Draw Obstacles
                for (int i = 0; i < MAX_OBJS; i++) {
                    int obj_x = current_level_data[i].x;
                    int obj_y = current_level_data[i].y;
                    int type  = current_level_data[i].type;
                    if (obj_x == -1) break;
                    
                    int screen_x = obj_x - scroll_x;
                    if (screen_x > -20 && screen_x < 320) {
                        if (type == TYPE_SPIKE) {
                            draw_triangle(screen_x, obj_y, 20, COLOR_SPIKE);
                        } else if (type == TYPE_WALL) {
                            draw_rect(screen_x, obj_y, 20, 20, COLOR_WALL);
                        } else if (type == TYPE_TRAP) {
                            draw_rect(screen_x, obj_y, 20, 20, COLOR_TRAP);
                        } else if (type == TYPE_PLAT) {
                            draw_rect(screen_x, obj_y, 40, 20, COLOR_PLAT); // Wide platform
                        }
                    }
                }
                
                // Goal
                int goal_screen_x = current_goal_x - scroll_x;
                if (goal_screen_x > -20 && goal_screen_x < 320) {
                    // Changed: Goal is now tall (200px) and Green (COLOR_WIN)
                    draw_rect(goal_screen_x, FLOOR_Y - 200, 20, 200, COLOR_WIN);
                }
                draw_rect(cube_x, cube_y, cube_size, cube_size, COLOR_CUBE);
            }

            wait_for_vsync();
            back_buffer_index = !back_buffer_index;
            *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);
            
            // --- NEW: Restart Logic ---
            if (game_over || level_complete) {
                // If the Jump Button is pressed, break to the main menu
                if (*BUTTONS & 1) {
                    // Debounce: Wait for release before leaving
                    while(*BUTTONS & 1);
                    break; // Breaks the inner Game Loop, restarts the Outer Loop
                }
            }
        }
    }
}