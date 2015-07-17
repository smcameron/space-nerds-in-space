include <wedge.scad>;

/* loosley modeled after cbase hackerspace space station */


module outer_ring()
{
	difference() {
		union() {
			scale(v = [1, 1, 0.1])
				rotate_extrude(height = 5, angle = 250, convexity = 10)
					translate([5, 0, 0])
						circle(r = 4, $fn = 10);
			translate([0, 0, 0.20])
			rotate_extrude(height = 0.5, angle=250, convexity=10, $fn=32)
					translate([7.50, 0, 0])
						circle(r = 0.35, $fn=8);
		}
		cylinder(h = 10, r1 = 5.25, r2 = 5.25, center = true);
		translate(v = [0, 0, -2])
			wedge(5, 10, 120);
	}
}

module central_hub()
{
	difference() {
		union() {
			cylinder(h = 0.5, r1 = 2.5, r2 = 2, $fn=12);
			rotate(a = 180, v = [0, 1, 0])
				cylinder(h = 0.5, r1 = 2.5, r2 = 2, $fn=12);
		}
		cylinder(h = 2, r1 = 1, r2 = 1, center = true, $fn = 12);
	}
	cylinder(h = 3, r1 = 0.2, r2 = 0.1);
	cylinder(h = 0.25, r1 = 2.1, r2 = 2.1, center = true);
}

module spoke()
{
	difference() {
	rotate(a = -5, v = [0, 0, 1])
		wedge(0.5, 9, 10);
	rotate(a = 2.25, v = [0, 1, 0])
		translate(v = [0, -4.5, 0.5])
			cube(size = [9, 9, 0.5]);
	}
	rotate(a = -5, v = [0, 0, 1])
		wedge(0.6, 9, 3);
	rotate(a = 5, v = [0, 0, 1])
		wedge(0.6, 9, 3);
	translate(v = [3.2, 0, 0])
			cube(size = [0.75, 0.7, 1.2], center = true);
}

module spokes()
{
	union() {
		spoke(0.5, 9, 10);
		rotate(a = 120, v = [0, 0, 1])
			spoke(0.5, 9, 10);
		rotate(a = -120, v = [0, 0, 1])
			spoke(0.5, 9, 10);
	}
}

scale(v = [10, 10, 10])
union() {
	outer_ring();
	central_hub();
	spokes();
}

use <imposter_docking_port.scad>;
docking_ports = 1;
if (docking_ports) {
	docking_port3(32, 0, -5, 0, 1, 0, -90, 2);
	docking_port3(-16, -27, -5, 0, 1, 0, -90, 2);
	docking_port3(-16, 27, -5, 0, 1, 0, -90, 2);
}

