// ============================================================================
//  Tester Present  TP-CAN-2I  -- Rugged Sealed Enclosure (parametric)
//  (c) 2026 Jack Leighton - Designed in Australia
//
//  Two-part automotive enclosure for the Programmable Dual-CAN Inline Interface.
//  Base tray + lid, perimeter gasket seal (IP65 target), four heavy-duty corner
//  mounting ears (M5 + steel eyelet), Deutsch DT-12 bulkhead cutout, sealed USB-C
//  service port, PCB standoffs (M3 brass inserts), and 5 LED light-pipes.
//
//  RENDER / EXPORT (OpenSCAD):
//    set  part = "base"  | "lid" | "assembly" | "print"
//    F6 (render) then  File > Export > STL.  Export base and lid separately for SLA.
//  Units: millimetres.
// ============================================================================

part = "assembly";          // "base" | "lid" | "assembly" | "print"
$fn = 64;

// ---- PCB -------------------------------------------------------------------
pcb_x      = 70;            // PCB length  (matches KiCad board)
pcb_y      = 50;            // PCB width
pcb_t      = 1.6;
pcb_clear  = 2.0;           // gap PCB edge -> inner wall
pcb_hole_inset = 4.0;       // PCB mounting-hole inset (matches generate_board.py)

// ---- Shell -----------------------------------------------------------------
wall       = 3.0;           // side-wall thickness (rugged)
floor_t    = 3.0;           // base floor thickness
lid_top_t  = 3.0;           // lid top thickness
corner_r   = 4.0;           // outer vertical corner radius
standoff_h = 4.0;           // PCB sits this high above the floor
head_room  = 16.0;          // clearance above PCB top for tall parts (DCDC, caps)
gasket_w   = 1.6;           // gasket groove width
gasket_d   = 1.4;           // gasket groove depth
lid_lip_h  = 4.0;           // lid tongue depth into the seal
fit_gap    = 0.20;          // assembly clearance

// ---- Derived inner / outer -------------------------------------------------
inner_x = pcb_x + 2*pcb_clear;            // 74
inner_y = pcb_y + 2*pcb_clear;            // 54
outer_x = inner_x + 2*wall;               // 80
outer_y = inner_y + 2*wall;               // 60
cavity_h = standoff_h + pcb_t + head_room; // 21.6
base_h  = floor_t + cavity_h;             // 27.6  (rim height)

// PCB origin inside the cavity (case coords, lower-left of inner cavity = (wall,wall))
pcb_ox = wall + pcb_clear;                // 5
pcb_oy = wall + pcb_clear;                // 5

// PCB mounting-hole centres (case coords)
pcb_holes = [
  [pcb_ox + pcb_hole_inset,         pcb_oy + pcb_hole_inset],
  [pcb_ox + pcb_x - pcb_hole_inset, pcb_oy + pcb_hole_inset],
  [pcb_ox + pcb_hole_inset,         pcb_oy + pcb_y - pcb_hole_inset],
  [pcb_ox + pcb_x - pcb_hole_inset, pcb_oy + pcb_y - pcb_hole_inset],
];

// ---- Fasteners / inserts ---------------------------------------------------
m3_insert_d = 4.0;          // heat-set brass insert pilot (M3)
m3_clear_d  = 3.4;          // M3 screw clearance
m3_head_d   = 6.2;          // M3 cap-head counterbore
m5_clear_d  = 5.5;          // M5 vehicle-mount clearance
eyelet_od   = 10.0;         // steel eyelet outer (anti-crush)

// ---- Corner lid-screw towers (inside, outboard of seal) --------------------
tower_d   = 9.0;
// tower centres tucked into the four outer corners
tw = outer_x/2 - corner_r - 1.0;
th = outer_y/2 - corner_r - 1.0;
tower_pos = [ [corner_r+1.5, corner_r+1.5],
              [outer_x-corner_r-1.5, corner_r+1.5],
              [corner_r+1.5, outer_y-corner_r-1.5],
              [outer_x-corner_r-1.5, outer_y-corner_r-1.5] ];

// ---- Connector (Deutsch DT-12 flange receptacle) ---------------------------
//  Bulkhead cutout on the +X end wall.  VERIFY against the exact DT04-12PA
//  flange-receptacle drawing before tooling.
conn_w   = 25.0;            // cutout width  (Y)
conn_h   = 21.0;            // cutout height (Z)
conn_z   = floor_t + 6.0;   // cutout bottom above floor
conn_flange_screw = 3.3;    // DT flange mount screws
conn_flange_dx = 31.0;      // flange screw spacing (Y)

// ---- USB-C service port (sealed with rubber bung) --------------------------
usb_w = 13.0; usb_h = 8.0;
usb_x = pcb_ox + 6.0;       // aligns with J1 (USB-C) near left edge
usb_z = floor_t + standoff_h + pcb_t;   // at PCB top plane

// ---- LED light-pipes (5) in the lid ----------------------------------------
//  PCB LEDs MUST be placed to match these XY centres (case coords):
led_d = 3.2;
led_y = outer_y - 12.0;
led_xs = [26, 31, 36, 41, 46];   // PWR, Wi-Fi, CAN1, CAN2, STATUS

// ============================================================================
//  Helpers
// ============================================================================
module rbox(x, y, z, r) {           // prism with vertical rounded edges
  hull() for (sx=[r, x-r], sy=[r, y-r]) translate([sx, sy, 0]) cylinder(h=z, r=r);
}

module corner_ear(p, ang) {         // heavy-duty mounting ear with eyelet
  ear_len = 13.0; ear_t = 8.0; ear_w = 14.0;
  translate(p) rotate([0,0,ang]) {
    difference() {
      union() {
        // tab
        hull() {
          cylinder(h=ear_t, d=eyelet_od+3);
          translate([ear_len,0,0]) cylinder(h=ear_t, d=ear_w);
        }
      }
      // M5 hole through the outer end + eyelet pocket
      translate([ear_len,0,-1]) cylinder(h=ear_t+2, d=m5_clear_d);
      translate([ear_len,0,ear_t-2.5]) cylinder(h=3, d=eyelet_od);   // eyelet flush pocket
    }
  }
}

// ============================================================================
//  BASE
// ============================================================================
module base() {
  difference() {
    union() {
      // outer shell
      rbox(outer_x, outer_y, base_h, corner_r);
      // mounting ears (diagonal from each corner)
      corner_ear([0,0],            225);
      corner_ear([outer_x,0],      315);
      corner_ear([0,outer_y],      135);
      corner_ear([outer_x,outer_y], 45);
    }
    // hollow cavity
    translate([wall, wall, floor_t])
      rbox(inner_x, inner_y, base_h, max(0.5, corner_r-wall));
    // gasket groove in the top rim
    translate([0,0,base_h-gasket_d])
      difference() {
        rbox(outer_x, outer_y, gasket_d+1, corner_r);
        translate([wall*0.5, wall*0.5, -1])
          rbox(outer_x-wall, outer_y-wall, gasket_d+3, corner_r-0.5);
        translate([wall*0.5+gasket_w, wall*0.5+gasket_w, -1])
          rbox(outer_x-wall-2*gasket_w, outer_y-wall-2*gasket_w, gasket_d+3, corner_r-1);
      }
    // connector cutout on +X end wall
    translate([outer_x-wall-1, outer_y/2-conn_w/2, conn_z])
      cube([wall+2, conn_w, conn_h]);
    for (s=[-1,1]) translate([outer_x-wall-1, outer_y/2 + s*conn_flange_dx/2, conn_z+conn_h/2])
      rotate([0,90,0]) cylinder(h=wall+2, d=conn_flange_screw);
    // USB-C service port on -Y side wall
    translate([usb_x-usb_w/2, -1, usb_z-usb_h/2]) cube([usb_w, wall+2, usb_h]);
  }

  // PCB standoffs (inside)
  for (p=pcb_holes) translate([p[0], p[1], floor_t])
    difference() {
      cylinder(h=standoff_h, d=6.5);
      translate([0,0,-0.5]) cylinder(h=standoff_h+1, d=m3_insert_d);
    }

  // lid-screw towers (inside the corners, up to rim)
  for (p=tower_pos) translate([p[0], p[1], floor_t])
    difference() {
      cylinder(h=cavity_h-gasket_d, d=tower_d);
      translate([0,0,cavity_h-gasket_d-6]) cylinder(h=7, d=m3_insert_d);
    }
}

// ============================================================================
//  LID
// ============================================================================
module lid() {
  lip_inset = wall*0.5 + gasket_w + fit_gap;
  difference() {
    union() {
      rbox(outer_x, outer_y, lid_top_t, corner_r);              // top plate
      // sealing tongue that drops into the gasket channel
      translate([wall*0.5+gasket_w+fit_gap, wall*0.5+gasket_w+fit_gap, -lid_lip_h])
        difference() {
          rbox(outer_x-wall-2*gasket_w-2*fit_gap, outer_y-wall-2*gasket_w-2*fit_gap, lid_lip_h, corner_r-1);
          translate([wall, wall, -1])
            rbox(outer_x-3*wall, outer_y-3*wall, lid_lip_h+2, corner_r-1);
        }
    }
    // corner screw holes (counterbored M3, into towers)
    for (p=tower_pos) translate([p[0], p[1], -lid_lip_h-1]) {
      cylinder(h=lid_top_t+lid_lip_h+2, d=m3_clear_d);
      translate([0,0,lid_lip_h+1]) cylinder(h=lid_top_t, d=m3_head_d); // counterbore from top
    }
    // LED light-pipe holes
    for (x=led_xs) translate([x, led_y, -lid_lip_h-1]) cylinder(h=lid_top_t+lid_lip_h+2, d=led_d);
    // recessed branding pocket for a printed/engraved label (top)
    translate([outer_x/2, outer_y/2, lid_top_t-0.6])
      linear_extrude(1.0)
        text("TESTER PRESENT", size=4.5, halign="center", valign="center", font="Liberation Mono:style=Bold");
  }
}

// ============================================================================
//  Render selector
// ============================================================================
module pcb_ghost() {                // visual aid only
  color([0.05,0.25,0.05,0.6])
    translate([pcb_ox, pcb_oy, floor_t+standoff_h]) cube([pcb_x, pcb_y, pcb_t]);
}

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "assembly") {
  base(); pcb_ghost();
  color([0.1,0.1,0.1,0.85]) translate([0,0,base_h+12]) lid();   // exploded up
}
else if (part == "print") {         // both parts laid flat for the print bed
  base();
  translate([0, outer_y+15, lid_top_t]) rotate([180,0,0]) lid();
}
