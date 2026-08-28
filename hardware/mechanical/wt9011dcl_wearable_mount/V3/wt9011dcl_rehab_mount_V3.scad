// WT9011DCL-BT50 rehabilitation wearable enclosure + strap holder V3
// Revised from physical print feedback (2026-08-20)
// Units: mm. X = short side, Y = long side. Type-C is on -Y face.
$fn = 72;

PART = "bottom"; // bottom | lid | holder | assembly

// ---------------- Sensor ----------------
sensor_x = 23.5;
sensor_y = 32.5;
sensor_z = 11.4;

// ---------------- Enclosure ----------------
case_x = 29.6;
case_y = 38.6;
case_r = 6.2;

// Bottom is kept at the proven outside size, but the upper neck is now
// substantially smaller so the lid can actually sleeve over it.
bottom_h = 7.8;
lower_h = 5.2;
floor_t = 2.0;
neck_x = 27.0;
neck_y = 36.0;
neck_r = 5.1;

cavity_x = 24.0;
cavity_y = 33.0;
cavity_r = 4.7;

// Lid: same finished outline as the lower body, but a 0.95-1.0 mm skirt
// sleeves over the smaller neck with generous FDM clearance.
lid_h = 9.85;
lid_top_t = 1.25;
lid_overlap_depth = 2.75;
lid_overlap_x = 27.7; // 0.35 mm clearance per side vs neck
lid_overlap_y = 36.7;
lid_overlap_r = 5.45;
lid_cavity_x = 25.8;
lid_cavity_y = 34.8;
lid_cavity_r = 5.0;

// Top controls, referenced to sensor center
button_x = 0;
button_y = -3.2;
button_hole_d_inner = 9.4;
button_hole_d_top = 11.2;

// V3: indicator opening shifted +1.2 mm toward the arrow (+Y) and slightly
// enlarged for printing tolerance. Button opening remains unchanged.
led_x = 7.0;
led_y = -6.1;
led_hole_d = 4.2;

// Type-C opening: keep the established centerline, with a little more plug clearance.
port_w = 12.5;
port_h = 6.4;
port_r = 1.6;
port_global_z = 6.35;

// Lid anti-drop snap: four detents in the neck + four integrated wedge bumps in skirt.
snap_y = 8.3;
snap_len = 5.2;
snap_recess_depth = 0.45;
snap_recess_h = 0.95;
snap_global_z = 6.65;
snap_bump_depth = 0.55;

// ---------------- Holder ----------------
// Follows the supplied reference concept: central support plate + left/right
// strap slots + four one-piece claws that rise OUTSIDE the enclosure and hook
// over the enclosure top surface. No enclosure side-groove retention is used.
holder_x = 50.0;
holder_y = 44.0;
holder_r = 6.0;
holder_t = 2.4;
strap_slot_w = 5.0;
strap_slot_len = 25.0;
strap_slot_r = 2.45;
strap_slot_x = 20.4;

holder_clear = 0.35;
arm_x = 10.3;          // keeps the center Type-C region clear
arm_w = 5.6;
arm_t = 2.2;
arm_inner_y = case_y/2 + holder_clear;
arm_outer_y = arm_inner_y + arm_t;
arm_hook_depth = 2.0;
case_assembled_h = lower_h + lid_h; // 15.05 mm from enclosure bottom to lid top
hook_bottom = case_assembled_h - 0.45; // 0.45 mm top overlap for retention
hook_top = case_assembled_h + 0.75;

// ---------------- Helpers ----------------
module rounded_rect_2d(w,h,r) {
    offset(r=r) square([w-2*r,h-2*r],center=true);
}

module rounded_prism(w,h,z,r) {
    linear_extrude(height=z) rounded_rect_2d(w,h,r);
}

module rounded_slot_2d(w,h,r) {
    rounded_rect_2d(w,h,min(r,min(w,h)/2-0.01));
}

// Opening on -Y face. 2D geometry is X-Z, extruded through Y.
module side_rounded_opening(width,height,radius,zc,y_start=-50,depth=100) {
    translate([0,y_start,zc])
        rotate([90,0,0])
            linear_extrude(height=depth)
                rounded_slot_2d(width,height,radius);
}

// Four shallow snap recesses cut into +/-X faces of the upper neck.
module neck_snap_recesses() {
    zc = snap_global_z;
    for (sx=[-1,1], yy=[-snap_y,snap_y]) {
        translate([sx*(neck_x/2 - snap_recess_depth/2 + 0.03), yy, zc])
            cube([snap_recess_depth+0.08, snap_len, snap_recess_h], center=true);
    }
}

module bottom_case() {
    difference() {
        union() {
            rounded_prism(case_x,case_y,lower_h,case_r);
            translate([0,0,lower_h])
                rounded_prism(neck_x,neck_y,bottom_h-lower_h,neck_r);
        }

        // Sensor tray cavity.
        translate([0,0,floor_t])
            rounded_prism(cavity_x,cavity_y,bottom_h-floor_t+0.4,cavity_r);

        // Common Type-C opening referenced to assembled Z.
        side_rounded_opening(port_w,port_h,port_r,port_global_z,-case_y/2-1.0,6.0);

        // New lid retention recesses.
        neck_snap_recesses();
    }

    // Anti-rattle ribs retain the sensor body without allowing angular play.
    rib_in = 0.10;
    rib_t = 0.45;
    rib_z0 = floor_t+0.25;
    rib_h = bottom_h-floor_t-0.55;
    for (sx=[-1,1], yy=[-7.5,7.5])
        translate([sx*(cavity_x/2-rib_in/2),yy,rib_z0+rib_h/2])
            cube([rib_t,4.6,rib_h],center=true);
    for (sy=[-1,1], xx=[-5.7,5.7])
        translate([xx,sy*(cavity_y/2-rib_in/2),rib_z0+rib_h/2])
            cube([4.4,rib_t,rib_h],center=true);
}

// Integrated inward snap bump on an +/-X inner skirt face.
module one_lid_snap_bump(sx=1, yy=0) {
    inner_x = lid_overlap_x/2;
    z0 = snap_global_z - lower_h - snap_recess_h/2;
    // Ramp: thin at the lower insertion edge, full depth at the upper edge.
    // It overlaps the surrounding skirt solid, so it is a single body.
    if (sx > 0) {
        hull() {
            translate([inner_x-0.12, yy-snap_len/2+0.25, z0])
                cube([0.12,snap_len-0.5,0.18]);
            translate([inner_x-snap_bump_depth, yy-snap_len/2+0.25, z0+0.50])
                cube([snap_bump_depth,snap_len-0.5,0.36]);
        }
        // Short upper catch face improves pull-off resistance.
        translate([inner_x-snap_bump_depth,yy-snap_len/2+0.25,z0+0.50])
            cube([snap_bump_depth,snap_len-0.5,0.38]);
    } else {
        hull() {
            translate([-inner_x, yy-snap_len/2+0.25, z0])
                cube([0.12,snap_len-0.5,0.18]);
            translate([-inner_x, yy-snap_len/2+0.25, z0+0.50])
                cube([snap_bump_depth,snap_len-0.5,0.36]);
        }
        translate([-inner_x,yy-snap_len/2+0.25,z0+0.50])
            cube([snap_bump_depth,snap_len-0.5,0.38]);
    }
}

module lid_case() {
    difference() {
        rounded_prism(case_x,case_y,lid_h,case_r);

        // Deep sleeve cavity: wide enough to actually pass over the bottom neck.
        translate([0,0,-0.05])
            rounded_prism(lid_overlap_x,lid_overlap_y,lid_overlap_depth+0.12,lid_overlap_r);

        // Upper cavity over the sensor, leaving only a 1.25 mm top skin.
        translate([0,0,lid_overlap_depth-0.35])
            rounded_prism(lid_cavity_x,lid_cavity_y,
                lid_h-lid_top_t-(lid_overlap_depth-0.35)+0.05,lid_cavity_r);

        // Type-C opening in the lid, using the same assembled Z datum.
        side_rounded_opening(port_w,port_h,port_r,
            port_global_z-lower_h,-case_y/2-1.0,6.0);

        // Main button opening unchanged.
        translate([button_x,button_y,lid_h-lid_top_t-0.05])
            cylinder(h=lid_top_t+0.2,d1=button_hole_d_inner,d2=button_hole_d_top);

        // Indicator opening shifted upward (+Y).
        translate([led_x,led_y,lid_h-lid_top_t-0.05])
            cylinder(h=lid_top_t+0.2,d=led_hole_d);
    }

    // Four integrated snap bumps engage the neck recesses.
    for (sx=[-1,1], yy=[-snap_y,snap_y])
        one_lid_snap_bump(sx,yy);

    // Orientation arrow retained.
    translate([0,10.8,lid_h])
        linear_extrude(height=0.35)
            polygon(points=[[-2.6,-2.2],[2.6,-2.2],[0,2.5]]);
}

// Extrude a Y-Z polygon along X by 'width'.
module yz_prism(profile,width) {
    rotate([0,90,0])
        linear_extrude(height=width,center=true,convexity=10)
            polygon(points=[for(p=profile) [-p[1],p[0]]]);
}

// One positive-Y claw. Negative side is mirrored.
module holder_claw_pos(xpos) {
    // Main spring arm + top hook as a single profile.
    profile = [
        [arm_outer_y,0],
        [arm_inner_y,0],
        [arm_inner_y,hook_bottom],
        [arm_inner_y-arm_hook_depth,hook_bottom],
        [arm_inner_y-arm_hook_depth,hook_bottom+0.55],
        [arm_inner_y,hook_top],
        [arm_outer_y,hook_top]
    ];

    translate([xpos,0,holder_t-0.30])
        yz_prism(profile,arm_w);

    // Wide, overlapping root block: prevents the four claws from behaving like
    // separately printed posts and greatly increases break-off resistance.
    translate([xpos,arm_inner_y+0.45,holder_t+1.25])
        cube([arm_w+2.0,4.1,3.1],center=true);
}

module holder_claw(xpos,sy=1) {
    if (sy > 0) holder_claw_pos(xpos);
    else mirror([0,1,0]) holder_claw_pos(xpos);
}

module strap_holder() {
    difference() {
        rounded_prism(holder_x,holder_y,holder_t,holder_r);

        // Reference-style left/right strap slots, enlarged for the new enclosure.
        for (sx=[-1,1])
            translate([sx*strap_slot_x,0,-0.1])
                linear_extrude(height=holder_t+0.2)
                    rounded_slot_2d(strap_slot_w,strap_slot_len,strap_slot_r);
    }

    // Four ONE-PIECE claws outside the enclosure. They hook over the TOP surface,
    // rather than engaging shallow grooves on the enclosure side.
    for (xx=[-arm_x,arm_x], sy=[-1,1])
        holder_claw(xx,sy);

    // Two short side locating rails prevent left-right motion while leaving the
    // enclosure removable. These are not retention grooves and do not require
    // matching slots in the enclosure.
    guide_h = 1.15;
    guide_t = 1.15;
    guide_len = 18.0;
    for (sx=[-1,1]) {
        gx = sx*(case_x/2 + holder_clear + guide_t/2);
        translate([gx,0,holder_t+guide_h/2-0.15])
            cube([guide_t,guide_len,guide_h+0.3],center=true);
    }
}

module sensor_dummy() {
    color([0.08,0.08,0.08,0.7])
        translate([0,0,floor_t])
            rounded_prism(sensor_x,sensor_y,sensor_z,4.6);
}

module assembly() {
    color([0.72,0.70,0.65]) bottom_case();
    color([0.80,0.78,0.72]) translate([0,0,lower_h]) lid_case();
    sensor_dummy();
    color([0.80,0.78,0.72]) translate([0,0,-holder_t]) strap_holder();
}

if (PART=="bottom") bottom_case();
else if (PART=="lid") lid_case();
else if (PART=="holder") strap_holder();
else if (PART=="assembly") assembly();
