$fn=12;

module spoke(angle)
{
	rotate(a = angle, v = [0, 0, 1]) {
		translate(v = [17, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				rotate(a = 90, v = [0, 0, 1])
					scale(v = [1, 0.5, 1])
						cylinder(h = 57, r1 = 3, r2 = 1, center = true);
		translate(v = [45, 0, 0]) {
			rotate(a = 90, v = [1, 0, 0]) {
				cylinder(h = 28, r1 = 4, r2 = 4, $fn=20, center = true);
			}
			translate(v = [0, -14, 0]) {
				rotate(a = 90, v = [1,0,0])
					translate(v = [0,0,-1])
						cylinder(h = 2, r1 = 4.5, r2 = 4.5, $fn=20, center = true);
				sphere(r = 4, $fn=20);
			}
			translate(v = [0, 14, 0]) {
				rotate(a = 90, v = [1,0,0])
					translate(v = [0,0,1])
						cylinder(h = 2, r1 = 4.5, r2 = 4.5, $fn=20, center = true);
				sphere(r = 4, $fn=20);
			}
		}
		translate(v = [-15, 0, 0])
			scale(v = [1, 1, 1])
				sphere(r = 5);
	}
}

scale(v = [2.0, 2.0, 2.0]) {
	union() {
	difference() {
		union() {
			cylinder(h = 5, r1 = 30, r2 = 35, center = true);
			translate(v = [0, 0, 2.5])
				cylinder(h = 1, r1 = 35, r2 = 35, center = true);
			translate(v = [0, 0, 5])
				cylinder(h = 5, r1 = 35, r2 = 30, center = true);
		}
		cylinder(h = 20, r1 = 28, r2 = 28, center = true);
	}

	cylinder(h = 10, r1 = 7, r2 = 5, center = true);
	translate(v = [0, 0, -9])
	cylinder(h = 10, r1 = 5, r2 = 7, center = true);
	translate(v = [0, 0, 10])
	cylinder(h = 30, r1 = 0.8, r2 = 0.5, center = true);

	translate(v = [49, 6, 0]) {
		cube(size = [2, 3, 2]);
		cube(size = [20, 0.5, 0.5]);
	}

	rotate(a = 69, v = [0, 0, 1])
	translate(v = [30, 6, 0]) {
		rotate(a = 90, v = [0, 1, 0]) {
			cube(size = [7, 2, 2]);
			cube(size = [15, 0.5, 0.5]);
		}
	}

	spoke(0);
	spoke(360 / 3);
	spoke(-360 / 3);
	}
}

use <imposter_docking_port.scad>;
docking_ports = false;
if (docking_ports) {
	docking_port(90, -37, 0, 0, 0, 1, 90, 0.3);
	docking_port(-13, 97, 0, 0, 0, 1, 210, 0.3);
	docking_port(-77, -60, 0, 0, 0, 1, -30, 0.3);
}

