#include <stdint.h>
#include <stdbool.h>

// --- Hardware Addresses ---
volatile int *VGA_CTRL = (volatile int*) 0x04000100;
volatile char *VGA     = (volatile char*) 0x08000000;
volatile int *BUTTONS  = (volatile int*) 0x040000d0; 
volatile int *SWITCHES = (volatile int*) 0x04000010; // Base address for SW0-SW9

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
#define COLOR_MENU_TEXT 0xFF // White for menu

#define START_DELAY_FRAMES 120 // 2 seconds delay after level start

// --- LEVEL DATA ---
#define MAX_LEVELS 5
#define MAX_SPIKES 30 // Max spikes possible in any level

void handle_interrupt (unsigned _irq)
{ }

// 1. Spike Positions for each level
// -1 indicates end of spike data for that level
int ALL_LEVELS_SPIKES[MAX_LEVELS][MAX_SPIKES] = {
    // Level 1: Easy (Warm up)
    { 600, 1000, 1400, 2000, 2500, -1 }, 
    
    // Level 2: Medium (More frequency)
    { 500, 900, 1200, 1600, 2000, 2500, 3000, -1 },
    
    // Level 3: Hard (Double spikes introduced)
    { 500, 900, 1200, 1250, 1800, 2400, 2450, 3000, 3500, -1 },
    
    // Level 4: Insane (Tricky gaps)
    { 500, 800, 850, 1400, 1450, 1800, 2200, 2250, 2300, 3000, 3050, 3800, -1 },
    
    // Level 5: Demon (Endurance)
    { 500, 700, 750, 1100, 1500, 1550, 1600, 2000, 2200, 2250, 2600, 3000, 3050, 3100, 3500, 3550, 4000, 4050, 4100, 4500, -1 }
};

// 2. Goal Positions (Length of level)
int LEVEL_GOALS[MAX_LEVELS] = { 3000, 3500, 4000, 4500, 5000 };

// 3. Scroll Speed for each level (Harder levels = Faster)
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

// --- NEW: Level Indicator HUD ---
// Draws 5 boxes at the top, highlights the current one
void draw_level_indicator(int current_level) {
    int start_x = 125; // Centered roughly: (320 - 70) / 2
    int y = 10;
    int size = 10;
    
    for (int i = 0; i < 5; i++) {
        int x = start_x + (i * 15); // 15px spacing
        
        // Highlight if this is the active level
        // Use Cyan for active, Dim Blue for inactive
        char color = (i == current_level) ? COLOR_GOAL : 0x01; 
        
        draw_rect(x, y, size, size, color);
        
        // Add a white dot in the center of the active level for extra visibility
        if (i == current_level) {
            draw_rect(x + 3, y + 3, 4, 4, 0xFF);
        }
    }
}

bool check_collision(int box_x, int box_y, int box_s, int tri_x, int tri_y, int tri_h) {
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

// --- Menu Functions ---
void draw_menu_ui(int active_switch) {
    clear_screen(COLOR_BG);
    
    // Draw Title Bar
    draw_rect(0, 0, 320, 40, 0x01); // Dark Blue Header

    // Draw Level Select Boxes
    for(int i=0; i<5; i++) {
        int x = 40 + (i * 50);
        int y = 100;
        int size = 30;
        
        // Color is Green if switch active, else Red
        char color = (active_switch == i) ? COLOR_WIN : COLOR_CUBE; 
        
        draw_rect(x, y, size, size, color);
        
        // Draw simple visual indicator for level number (white dots inside)
        for(int dot=0; dot<=i; dot++) {
            plot_pixel(x + 5 + (dot*4), y + 15, 0xFF);
        }
    }

    // Instructions
    draw_rect(0, 200, 320, 2, 0xFF); // Line at bottom
}

int main() {
    // --- PHASE 1: LEVEL SELECTION ---
    int selected_level = -1;
    
    // Menu Loop
    while (selected_level == -1) {
        int switch_val = *SWITCHES;
        
        // Check Switches 0-4
        // We prioritize higher switches if multiple are pressed
        int current_selection = -1;
        if (switch_val & 0x01) current_selection = 0; // SW0
        if (switch_val & 0x02) current_selection = 1; // SW1
        if (switch_val & 0x04) current_selection = 2; // SW2
        if (switch_val & 0x08) current_selection = 3; // SW3
        if (switch_val & 0x10) current_selection = 4; // SW4
        
        // Draw the UI
        draw_menu_ui(current_selection);
        wait_for_vsync();
        
        // Swap Buffer
        back_buffer_index = !back_buffer_index;
        *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);

        // If a switch is active, select it and break
        if (current_selection != -1) {
            selected_level = current_selection;
            
            // Visual confirmation delay
            for(int k=0; k<30; k++) wait_for_vsync(); 
        }
    }

    // --- PHASE 2: GAME INITIALIZATION ---
    // Load data based on selected_level
    int *current_spikes = ALL_LEVELS_SPIKES[selected_level];
    int current_goal_x = LEVEL_GOALS[selected_level];
    int current_speed = LEVEL_SPEEDS[selected_level];

    // Player State
    int cube_x = 50; 
    int cube_y = FLOOR_Y - 20;
    int cube_dy = 0; 
    int cube_size = 20;
    bool is_grounded = true;

    // World State
    int scroll_x = 0; 

    // Game Flow
    bool game_over = false;
    bool level_complete = false;
    int frame_counter = 0; 

    // --- PHASE 3: GAME LOOP ---
    while (1) {
        
        if (!game_over && !level_complete && frame_counter > START_DELAY_FRAMES) {
            
            // A. INPUT & PHYSICS
            int button_pressed = (*BUTTONS & 1); 

            if (button_pressed && is_grounded) {
                cube_dy = JUMP_FORCE;
                is_grounded = false;
            }

            cube_y += cube_dy; 
            cube_dy += GRAVITY; 

            if (cube_y > FLOOR_Y - cube_size) {
                cube_y = FLOOR_Y - cube_size; 
                cube_dy = 0;                  
                is_grounded = true;           
            }

            // B. SCROLL THE WORLD
            scroll_x += current_speed;

            // C. CHECK COLLISIONS
            // Loop through the spikes for the CURRENT level
            for (int i = 0; i < MAX_SPIKES; i++) {
                int s_pos = current_spikes[i];
                if (s_pos == -1) break; // End of data

                int spike_screen_x = s_pos - scroll_x;

                // Optimization: Only check collision if spike is nearby
                if (spike_screen_x > -50 && spike_screen_x < 350) {
                     if (check_collision(cube_x, cube_y, cube_size, spike_screen_x, FLOOR_Y, 20)) {
                         game_over = true;
                     }
                }
            }

            // D. CHECK GOAL
            int goal_screen_x = current_goal_x - scroll_x;
            if (goal_screen_x <= cube_x) {
                level_complete = true; 
            }
        
        } else if (!game_over && !level_complete) {
            frame_counter++;
        }

        // --- DRAWING ---
        if (game_over) {
            clear_screen(COLOR_DEATH); 
        } else if (level_complete) {
            clear_screen(COLOR_WIN);   
        } else {
            clear_screen(COLOR_BG);    
        }

        if (!game_over && !level_complete) {
            // Draw Level Indicator
            draw_level_indicator(selected_level);

            // Floor
            for (int x = 0; x < SCREEN_W; x++) plot_pixel(x, FLOOR_Y, COLOR_FLOOR);

            // Spikes (Current Level)
            for (int i = 0; i < MAX_SPIKES; i++) {
                int s_pos = current_spikes[i];
                if (s_pos == -1) break;

                int spike_screen_x = s_pos - scroll_x;
                if (spike_screen_x > -20 && spike_screen_x < 320) {
                    draw_triangle(spike_screen_x, FLOOR_Y, 20, COLOR_SPIKE);
                }
            }

            // Goal
            int goal_screen_x = current_goal_x - scroll_x;
            if (goal_screen_x > -20 && goal_screen_x < 320) {
                draw_rect(goal_screen_x, FLOOR_Y - 60, 20, 60, COLOR_GOAL);
            }

            // Player
            char current_cube_color = (frame_counter < START_DELAY_FRAMES) ? COLOR_WAIT : COLOR_CUBE;
            draw_rect(cube_x, cube_y, cube_size, cube_size, current_cube_color);
        }

        wait_for_vsync();
        back_buffer_index = !back_buffer_index;
        *(VGA_CTRL + 1) = (unsigned int)VGA + (back_buffer_index ? (SCREEN_W * SCREEN_H) : 0);
        
        if (game_over || level_complete) {
             while(1); 
        }
    }
}