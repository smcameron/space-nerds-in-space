$fn = 16;

module hole(x, y, z, ra, rb, height)
{
		translate(v = [x, y, z])
			rotate(v = [1, 0, 0], a = 90)
				cylinder(r1 = ra, r2 = rb, h = height);
}

module hole_pair(x, y, z, ra, rb, height)
{
	hole(x, y, z, ra, rb, height);
	hole(x, y, -z, ra, rb, height);
}

module fuselage_half()
{
	difference() {
		union() {
			sphere(r = 22);
			scale(v = [1.7, 0.3, 1])
				sphere(r = 50);
		}
		translate(v = [0, 50, 0])
			cube(size = [300, 100, 100], center = true);
		translate(v = [-70, 0, 0])
			scale(v = [1, 1, 1.5])
				rotate(v = [0, 1, 0], a = 45)
					cube(size = [75, 75, 75], center = true);
		translate(v = [70, 0, 0])
			cube(size = [100, 40, 30], center = true);
		translate(v = [60, -12, -30])
			cube(size = [100, 10, 15], center = true);
		translate(v = [60, -12, 30])
			cube(size = [100, 10, 15], center = true);
		translate(v = [-60, 0, 0])
			cube(size = [40, 40, 100], center = true);
		translate(v = [95, 0, 0])
			cube(size = [40, 40, 100], center = true);
		hole_pair(0, -10, 30, 8, 9, 4);
		hole_pair(-18, -9, 33, 8, 9, 5);
	}
}

module fuselage()
{
	union() {
		translate(v = [0, -4, 0])
			scale(v = [1, 0.7, 1])
				fuselage_half();
		translate(v = [0, 4, 0])
			rotate(v = [1, 0, 0], a = 180)
				scale(v = [1, 0.7, 1])
					fuselage_half();
		rotate(v = [1, 0, 0], a = 90) {
			union() {
				cylinder(r1 = 10, r2 = 10, h = 40, center = true);
				cylinder(r1 = 20, r2 = 20, h = 20, center = true);
			}
		}
		translate(v = [-35, 0, 0]) {
			cube(size = [20, 40, 20], center = true);
			cube(size = [15, 35, 25], center = true);
		}
		cylinder(r1 = 8, r2 = 8, h = 105, center = true);
		translate(v = [-50, 0, 0])
			rotate(v = [0, 1, 0], a = 90) {
				difference() {
					cylinder(r1 = 8, r2 = 8, h = 110, center = true);
					translate(v = [0, 0, -50])
						cylinder(r1 = 10, r2 = 0, h = 30, center = true);
				}
			}
		translate(v = [-50, 0, 0])
			cube(size = [40, 5, 30], center = true);
		translate(v = [-80, 0, 0])
			rotate(v = [0, 1, 0], a = 90)
				cylinder(r1 = 10, r2 = 7, h = 10, center = true);
		translate(v = [-95, 0, 0])
			rotate(v = [0, 1, 0], a = 90)
				cylinder(r1 = 7, r2 = 10, h = 10, center = true);
		translate(v = [-8, 30, 0])
			cube(size = [1, 35, 1], center = true);
		translate(v = [-5, 20, 0])
			cube(size = [1, 35, 1], center = true);

	}
}

scale(v = [0.5, 0.5, 0.5])
	fuselage();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-56, 0, 0, 2.5);
}

