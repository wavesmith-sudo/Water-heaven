// CM5 Dongle Enclosure v1.2 - PARAMETRIC FIT-CHECK PROTOTYPE
// Reference: Waveshare CM5-NANO-B nominal PCB 55 x 41 mm.
// IMPORTANT: Connector locations, hole pattern, heatsink and antenna clearances NOT verified.
// Units: mm. Print shells open-side-up. No structural/thermal validation performed.
$fn=48;
part="assembly"; // "bottom", "top", "assembly", "board_template", "clearance_gauge"
L=90; W=55; H=25; corner=7;
wall=2; floor_t=2.2; top_t=2.2;
bottom_h=15; top_h=H-bottom_h;
fit=0.35;
board_l=55; board_w=41; board_t=1.6;
board_z=5.5; // board UNDERSIDE height from enclosure bottom
// board coordinates: centered in shell, connector locations are schematic
module rr(l,w,r) { hull() for(x=[-l/2+r,l/2-r]) for(y=[-w/2+r,w/2-r]) translate([x,y]) circle(r=r); }
module shell_outer(h){linear_extrude(height=h) rr(L,W,corner);}
module inner_cut(h){translate([0,0,-0.1]) linear_extrude(height=h+0.2) rr(L-2*wall,W-2*wall,corner-wall);}
module screw_holes(z0,h,rad){for(x=[-L/2+7,L/2-7]) for(y=[-W/2+7,W/2-7]) translate([x,y,z0]) cylinder(h=h,r=rad);}
module bottom(){
 difference(){
  union(){
   difference(){ shell_outer(bottom_h); translate([0,0,floor_t]) inner_cut(bottom_h); }
   // Four self-tapping screw pillars (pilot holes cut below)
   for(x=[-L/2+7,L/2-7]) for(y=[-W/2+7,W/2-7]) translate([x,y,floor_t]) cylinder(h=bottom_h-floor_t-1,r=3.3);
   // Board support pads only; no unverified hole-pattern assumptions
   for(x=[-board_l/2+5,board_l/2-5]) for(y=[-board_w/2+4,board_w/2-4]) translate([x,y,floor_t]) cylinder(h=board_z-floor_t,r=3.0);
   // internal side retainers for board (can be trimmed after fit test)
   for(y=[-board_w/2-1.3,board_w/2+1.3]) for(x=[-18,18]) translate([x,y,board_z-1]) cube([9,1.5,2],center=true);
  }
  screw_holes(floor_t, bottom_h, 1.05);
  // Long-side low intake slots, below split line
  for(x=[-24:8:24]) for(side=[-1,1]) translate([x,side*(W/2-wall/2),9]) cube([4.0,wall+1.5,4.0],center=true);
  // HDMI cable outlet at -X end; deliberately oversized for grommet / strain relief
  translate([-L/2,0,10]) cube([wall*3+2,13,8],center=true);
  // Provisional USB-C access at +X end; align with real board or replace with pigtail
  translate([L/2,0,9]) cube([wall*3+2,12,6],center=true);
 }
}
module top(){
 difference(){
  union(){
   difference(){shell_outer(top_h); translate([0,0,-0.1]) linear_extrude(height=top_h-top_t+0.1) rr(L-2*wall,W-2*wall,corner-wall);}
   // thin alignment lip enters lower shell, with clearance
   translate([0,0,-1.8]) difference(){
    linear_extrude(height=1.8) rr(L-2*wall-2*fit,W-2*wall-2*fit,corner-wall-fit);
    translate([0,0,-0.1]) linear_extrude(height=2) rr(L-2*wall-2*fit-2.0,W-2*wall-2*fit-2.0,corner-wall-fit-1.0);
   }
  }
  // Top ventilation slots; avoid wireless antenna area after module orientation is verified
  for(x=[-21:7:21]) translate([x,0,top_h-top_t-0.5]) cube([3.3,24,top_t+1],center=false);
  screw_holes(-2,top_h+4,1.65); // through-holes for ~M2 clearance
 }
}
// Simple physical fit gauge for the 55 x 41 mm PCB footprint. Not a connector template.
module clearance_gauge(){ difference(){ linear_extrude(2.4) rr(65,51,4); translate([0,0,-0.1]) linear_extrude(2.6) square([55.6,41.6],center=true); } }
module board_template(){
 color("forestgreen") translate([0,0,board_z+board_t/2]) cube([board_l,board_w,board_t],center=true);
}
if(part=="bottom") bottom();
else if(part=="top") top();
else if(part=="board_template") board_template();
else if(part=="clearance_gauge") clearance_gauge();
else {color("#333333") bottom(); translate([0,0,bottom_h]) color("#777777") top(); board_template();}
