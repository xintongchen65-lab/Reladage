// WT9011DCL-BT50 rehabilitation wearable enclosure + strap holder
// Units: mm
// Designed from WT9011DCL-BT50 nominal size 23.5 x 32.5 x 11.4 mm.
// Coordinate convention: X = short side, Y = long side; Type-C is on -Y face.
$fn = 64;

PART = "bottom"; // bottom | lid | holder | assembly

// --- Sensor nominal dimensions ---
sensor_x = 23.5;
sensor_y = 32.5;
sensor_z = 11.4;

// --- Enclosure parameters ---
case_x = 29.6;
case_y = 38.6;
corner_r = 6.2;
neck_x = 28.8;
neck_y = 37.8;
neck_r = 5.8;

bottom_h = 8.2;
main_h = 6.2;
floor_t = 2.0;

cavity_bottom_x = 24.0; // 0.25 mm clearance per X side before grip ribs
cavity_bottom_y = 33.0; // 0.25 mm clearance per Y side before grip ribs
cavity_r = 4.7;

lid_h = 9.2;
lid_top_t = 1.8;
lid_overlap_h = 2.2;
lid_overlap_x = 29.2;
lid_overlap_y = 38.2;
lid_overlap_r = 5.95;
lid_cavity_x = 24.2;
lid_cavity_y = 33.2;

// Port / control locations, referenced to sensor center
button_x = 0;
button_y = -3.2;
button_hole_d_inner = 9.4;
button_hole_d_top = 11.2;
led_x = 7.0;
led_y = -7.3;
led_hole_d = 3.8;
port_w = 12.0;
port_h = 6.2;
port_r = 1.6;

// Holder parameters
holder_x = 46.0;
holder_y = 41.0;
holder_r = 5.0;
holder_t = 2.4;
strap_slot_w = 5.5;
strap_slot_len = 27.0;
strap_slot_r = 2.3;
strap_slot_x = 19.0;
clip_y = 11.2;
clip_len_y = 5.6;
clip_wall_t = 1.55;
clip_h = 6.9;
clip_hook_depth = 0.85;
clip_hook_z0 = 4.95;
clip_hook_h = 1.15;

// Helper: 2D rounded rectangle centered on origin
module rounded_rect_2d(w, h, r) {
    offset(r=r) square([w-2*r, h-2*r], center=true);
}

module rounded_prism(w, h, z, r) {
    linear_extrude(height=z)
        rounded_rect_2d(w,h,r);
}

module rounded_slot_2d(w, h, r) {
    rounded_rect_2d(w,h,min(r,min(w,h)/2-0.01));
}

// Side opening on -Y face. The 2D geometry is X-Z, extruded through Y.
module side_rounded_opening(width, height, radius, zc, y_start=-50, depth=100) {
    translate([0, y_start, zc])
        rotate([90,0,0])
            linear_extrude(height=depth)
                rounded_slot_2d(width,height,radius);
}

// Side latch pocket on +/-X faces of lower case, centered at y.
module latch_pockets() {
    pocket_depth = 0.9;
    pocket_y = 5.2;
    pocket_z = 1.35;
    zc = 5.05;
    for (sx=[-1,1], yy=[-clip_y,clip_y]) {
        translate([sx*(case_x/2-pocket_depth/2+0.05), yy, zc])
            cube([pocket_depth+0.2,pocket_y,pocket_z],center=true);
    }
}

module bottom_case() {
    difference() {
        union() {
            // Flush lower body
            rounded_prism(case_x,case_y,main_h,corner_r);
            // Smaller neck captured by cap lid
            translate([0,0,main_h])
                rounded_prism(neck_x,neck_y,bottom_h-main_h,neck_r);
        }
        // Sensor cavity through the tray upper region
        translate([0,0,floor_t])
            rounded_prism(cavity_bottom_x,cavity_bottom_y,bottom_h-floor_t+0.3,cavity_r);
        // Type-C opening: lower half of assembled opening
        side_rounded_opening(port_w,3.7,1.3,6.35,-case_y/2-1,6);
        // Holder latch pockets
        latch_pockets();
    }

    // Anti-rattle grip ribs: 0.10 mm intrusion beyond nominal cavity walls.
    rib_in = 0.10;
    rib_t = 0.45;
    rib_z0 = floor_t+0.25;
    rib_h = 5.6;
    // X-side ribs (four)
    for (sx=[-1,1], yy=[-7.5,7.5]) {
        translate([sx*(cavity_bottom_x/2-rib_in/2),yy,rib_z0+rib_h/2])
            cube([rib_t,4.6,rib_h],center=true);
    }
    // Y-side ribs (four)
    for (sy=[-1,1], xx=[-5.7,5.7]) {
        translate([xx,sy*(cavity_bottom_y/2-rib_in/2),rib_z0+rib_h/2])
            cube([4.4,rib_t,rib_h],center=true);
    }
}

module lid_case() {
    difference() {
        rounded_prism(case_x,case_y,lid_h,corner_r);
        // Lower overlap cavity: slips over the bottom neck
        translate([0,0,-0.05])
            rounded_prism(lid_overlap_x,lid_overlap_y,lid_overlap_h+0.15,lid_overlap_r);
        // Upper sensor cavity, leaving lid top thickness
        translate([0,0,1.85])
            rounded_prism(lid_cavity_x,lid_cavity_y,lid_h-lid_top_t-1.85+0.05,cavity_r+0.1);
        // Type-C opening: upper half in cap (assembly global z = 6.2 + local z)
        side_rounded_opening(port_w,4.9,1.5,1.95,-case_y/2-1,6);
        // Button opening: tapered/countersunk for easy finger access
        translate([button_x,button_y,lid_h-lid_top_t-0.05])
            cylinder(h=lid_top_t+0.2,d1=button_hole_d_inner,d2=button_hole_d_top);
        // LED viewing opening
        translate([led_x,led_y,lid_h-lid_top_t-0.05])
            cylinder(h=lid_top_t+0.2,d=led_hole_d);
    }

    // Raised orientation arrow on top: points +Y (away from Type-C).
    translate([0,10.8,lid_h])
        linear_extrude(height=0.35)
            polygon(points=[[-2.6,-2.2],[2.6,-2.2],[0,2.5]]);
}

// Right-side flexible clip with inward hook; mirrored for left.
module one_clip(signx=1, yy=0) {
    x_inner = case_x/2 + 0.18;
    // clip wall outside the case
    x0 = signx>0 ? x_inner : -x_inner-clip_wall_t;
    translate([x0,yy-clip_len_y/2,holder_t])
        cube([clip_wall_t,clip_len_y,clip_h]);

    // Hook block into case latch pocket
    hook_x0 = signx>0 ? x_inner-clip_hook_depth : -x_inner;
    translate([hook_x0,yy-(clip_len_y-0.5)/2,holder_t+clip_hook_z0])
        cube([clip_hook_depth+(signx<0?0:0),clip_len_y-0.5,clip_hook_h]);

    // Lead-in ramp above hook for snap insertion
    // 2D X-Z triangular wedge extruded along Y.
    translate([0,yy-(clip_len_y-0.8)/2,0])
        rotate([90,0,0])
            linear_extrude(height=clip_len_y-0.8)
                polygon(points = signx>0 ?
                    [[x_inner,holder_t+clip_hook_z0+clip_hook_h],
                     [x_inner,holder_t+clip_h],
                     [x_inner-clip_hook_depth,holder_t+clip_hook_z0+clip_hook_h]] :
                    [[-x_inner,holder_t+clip_hook_z0+clip_hook_h],
                     [-x_inner,holder_t+clip_h],
                     [-x_inner+clip_hook_depth,holder_t+clip_hook_z0+clip_hook_h]]);
}

module strap_holder() {
    difference() {
        rounded_prism(holder_x,holder_y,holder_t,holder_r);
        // Two universal strap slots
        for (sx=[-1,1]) {
            translate([sx*strap_slot_x,0,-0.1])
                linear_extrude(height=holder_t+0.2)
                    rounded_slot_2d(strap_slot_w,strap_slot_len,strap_slot_r);
        }
        // Large center relief to reduce weight while preserving a support rim
        translate([0,0,-0.1])
            linear_extrude(height=holder_t+0.2)
                rounded_rect_2d(24.5,31.5,4.2);
    }

    // Four snap clips corresponding to four side latch pockets
    for (sx=[-1,1], yy=[-clip_y,clip_y])
        one_clip(sx,yy);

    // Four small support pads keep the enclosure plane stable above the relief.
    for (xx=[-9.8,9.8], yy=[-14.0,14.0])
        translate([xx,yy,holder_t]) cylinder(h=0.6,d=3.2);
}

module sensor_dummy() {
    color([0.1,0.1,0.1,0.8])
        translate([0,0,floor_t])
            rounded_prism(sensor_x,sensor_y,sensor_z,4.6);
}

module assembly() {
    color("beige") bottom_case();
    color("beige") translate([0,0,main_h]) lid_case();
    color("gray",0.55) sensor_dummy();
    color("beige") translate([0,0,-holder_t-0.6]) strap_holder();
}

if (PART=="bottom") bottom_case();
else if (PART=="lid") lid_case();
else if (PART=="holder") strap_holder();
else if (PART=="assembly") assembly();
