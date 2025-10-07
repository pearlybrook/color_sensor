NUM_FACES = 100;
// Value added or subtracted where applicable to get the preview planes to not
//  be inderterminate. 
RESOLVER = 0.0001;

// Idea is to have a sled for the MCU breakout board housing and positioning 
//  (for the USB port in particular) which is joined (preferably in a single
//  print rather than after the fact but this is negotiable) to the portion of
//  the apartus that sits inside the tube (housing the battery and at the front
//  the color sensor).
// Simplest way to design this is in two major parts, one to hold the MCU 
//  breakout and another to sit inside the tube. 
//
// Tube Dimensions:
//  ID: 1.315"
//
// C-Channel Dimensions:
//  Haven't settled on using the C-Channel, but we still have the space to do so
//      if desired so unless we have a reason not to it's best to keep on with
//      the idea that we may still use it.
//  Inner Depth:  1.875"
//  Inner Width:  ~1.750-1.9", varies considerably.
//  Length:       variable

mcu_housing();
//tube_insert();

module mcu_housing(){
// Dimensions for the front wall piece, the part that doesn't sit under the 
//  board and provides the attachment wall for the tube insert piece.
front_attach_wall_len = 0.250;

// Dimensions for the base, which is basically just a platform that everything
//  else rests on top of.
base_width = 1.633; // Matched to the width of the MCU breakout PCB.
base_len = 1.976 + front_attach_wall_len;
base_height = 0.125;

// Front attach wall dimensions continued.
front_attach_wall_width = base_width;
// ~0.925 from bottom of base plate to the top of the tube insert.
front_attach_wall_height = 0.925-base_height;

// Height of the breakout PCB supporting platforms.
support_platform_height = 0.250 - base_height;

// Side support platform dimensions. This is the portion of the aparatus that
//  sits underneath the side of the breakout PCB with the mounting holes in it.
mnt_side_support_platform_width = 0.150;
mnt_side_support_platform_len = base_len;

// Side support platform dimensions. This is the portion of the aparatus that
//  is opposite the breakout PCB mounting holes.
side_support_platform_width = 0.150;
side_support_platform_len = 1.250;

// Base plate.
translate([-base_width/2, -base_len/2, 0])
    cube([base_width, base_len, base_height]);

// Front attachment wall.
translate([
    -front_attach_wall_width/2, 
    base_len/2-front_attach_wall_len, 
    base_height
])
    cube([
        front_attach_wall_width, 
        front_attach_wall_len, 
        front_attach_wall_height
    ]);

// Mounting hole side support platform.
translate([-base_width/2, -mnt_side_support_platform_len/2, base_height])
    cube([
        mnt_side_support_platform_width, 
        mnt_side_support_platform_len, 
        support_platform_height
    ]);

// Non-mounting hole side support platform.
translate([
    base_width/2-side_support_platform_width, 
    -side_support_platform_len+base_len/2-front_attach_wall_len, 
    base_height
])
    cube([
        side_support_platform_width, 
        side_support_platform_len, 
        support_platform_height
    ]);
}

module tube_insert(){
// Need a cylidrical recess for the battery to sit in.
// Needs a face on it for mounting the color sensor breakout PCB on.
//  This needs a hollowed out middle for passing the JST connector through with
//      the in/out wiring running down the rest of the length.

// Battery:
//  ~2.545 from edge to edge, ~2.65" where the wire leads stick out.
//  0.716" at the largest point I can find.
//
// 3.5" from back of tube (assumed back of battery as well) to the back of the
//  color sensor. Tube end extends beyond this but this model doesn't need to
//  worry about that. 

// Main Body params:
len = 3.5;
rad = 1.310 / 2;

// Battery cutout params:
bat_recess_len = 2.7;
bat_recess_rad = 0.75 / 2;

// Color sensor params:
//  These params assume the color sensor is installed such that the JST 
//      connector has it's longest side parrallel to the ground. The JST 
//      connector is positioned off-center, so this also assumes the largest gap
//      between connector and PCB edge is 'down' i.e. in the same direction as
//      the battery recess cutout is. This is ~0.6" from the 'bottom' of the PCB
// These params concern the JST connector specifically, not exact dimensions of
//  it but the space needed for the cutout.
clr_sns_conn_cutout_cut_h = 0.4; // From OD to start of cut.
clr_sns_conn_cutout_w = 0.75; // 0.95" is the width of the breakout.
clr_sns_conn_cutout_h = 0.6; // Height of the actual cut.
clr_sns_conn_cutout_d = len-bat_recess_len; // How deep the clr sns cut is.

// To position the JST cutout we can translate from the Y+ extrema, move up at
//  least 0.6" (to reach bottom of JST ~exactly), and start the 'bottom' of the
//  cut there.
//  Best method for this would be to center the cut over the center of the JST
//      connector but idk how feasible this is with how weird these dimensions
//      are.
//  The cut needs to start before 0.6", this is easy, needs to end no shorter 
//      than 0.825" from there. We can oversize this, we will just use the other
//      dimensions to mount the PCB.

difference(){
        // Main body
        difference(){
            cylinder(h = len, r = rad, $fn=NUM_FACES);
    
            // Cuts off the top half of the housing, overtop the battery only,
            //  this makes it easier to size things and leaves room for wires
            //  w/o too much effort.
            translate([-rad, -rad, -RESOLVER]) 
                cube([rad*2, rad, bat_recess_len]);
        }// End diff

        //Battery recess cutout.
        translate([0, 0, -RESOLVER])
            cylinder(h = bat_recess_len, r = bat_recess_rad, $fn=NUM_FACES);

        // Color sensor wiring connector passthrough cutout.
        translate([
            -clr_sns_conn_cutout_w/2, 
            rad-clr_sns_conn_cutout_h-clr_sns_conn_cutout_cut_h, 
            len-clr_sns_conn_cutout_d-RESOLVER*2])
        cube([
            clr_sns_conn_cutout_w, 
            clr_sns_conn_cutout_h, 
            clr_sns_conn_cutout_d+RESOLVER*3]
        );
    }// End diff
}// End fcn