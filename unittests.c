#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

// --- Functions Under Test ---
// (Copied from game.c to allow offline testing)

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

// --- Test Suite ---

void test_box_collision() {
    printf("Testing Box Collision...\n");

    // Case 1: No Overlap (Far apart)
    // Cube at (0,0), Wall at (100,100)
    assert(check_collision_box(0, 0, 20, 100, 100, 20, 20) == false);

    // Case 2: Direct Hit (Perfect overlap)
    // Cube at (50,50), Wall at (50,50)
    assert(check_collision_box(50, 50, 20, 50, 50, 20, 20) == true);

    // Case 3: Edge Touching (Should hit)
    // Cube ends at x=20, Wall starts at x=19 (1px overlap)
    assert(check_collision_box(0, 0, 20, 19, 0, 20, 20) == true);

    // Case 4: Near Miss (1px gap)
    // Cube ends at x=20, Wall starts at x=21
    assert(check_collision_box(0, 0, 20, 21, 0, 20, 20) == false);

    // Case 5: Landing on Platform (Logic check)
    // Cube feet at y=100 (size 20, top 80), Platform top at y=100
    // This function only checks overlap. If feet exactly touch top, AABB might return false depending on > vs >=
    // Our logic uses: if (b1_bottom < b2_y) return false;
    // So 100 < 100 is False -> Collision TRUE.
    assert(check_collision_box(50, 80, 20, 50, 100, 50, 10) == true);

    printf("PASS: Box Collision\n");
}

void test_triangle_collision() {
    printf("Testing Triangle Collision...\n");

    int floor_y = 200;
    int spike_h = 20;
    
    // Case 1: Direct Hit
    // Cube in center of spike
    assert(check_collision_tri(100, floor_y - 20, 20, 110, floor_y, spike_h) == true);

    // Case 2: Hitbox Forgiveness
    // The spike is at x=150. Width is 20 (roughly). Left edge is 140. Right is 160.
    // Forgiveness is 4px. So effective hit area is 144 to 156.
    
    // Cube entirely before the spike (x=100)
    assert(check_collision_tri(100, floor_y - 20, 20, 150, floor_y, spike_h) == false);

    // Cube grazing the forgiveness zone (Real left edge is 140)
    // Cube right edge is at 142. Overlaps real triangle, but misses "Forgiving" hitbox (starts at 144).
    // Cube x = 122 -> Right edge = 142.
    assert(check_collision_tri(122, floor_y - 20, 20, 150, floor_y, spike_h) == false);

    // Cube hitting the "Forgiving" hitbox
    // Cube right edge at 145.
    // Cube x = 125 -> Right edge = 145. 145 > 144. Collision!
    assert(check_collision_tri(125, floor_y - 20, 20, 150, floor_y, spike_h) == true);

    // Case 3: Jumping Over
    // Cube x aligned with spike, but high in the air (y=100)
    assert(check_collision_tri(150, 100, 20, 150, floor_y, spike_h) == false);

    printf("PASS: Triangle Collision\n");
}

int main() {
    printf("--- STARTING UNIT TESTS ---\n");
    test_box_collision();
    test_triangle_collision();
    printf("--- ALL TESTS PASSED ---\n");
    return 0;
}