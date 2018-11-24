$fn = 16;

module fuselage_bulk()
{
	rotate(v = [1, 0, 0], a = 180)
		rotate(v = [0, 1, 0], a = -5)
		scale(v = [1.5, 1, 0.2])
			rotate(v = [0, 1, 0], a = 35)
				rotate(v = [0, 0, 1], a = 45)
					cube(size = [30, 30, 30], center = true);
}


module engine(x, y, z, r, l)
{
	translate(v = [x, y, z])
		rotate(v = [0, 1, 0], a = 90) {
			difference() {
				union() {
				cylinder(r1 = r, r2 = r, h = l, center = true);
				translate(v = [0, 0, l * 0.3])
					cylinder(r1 = r * 1.1, r2 = r * 1.1, h = l * 0.2, center = true);
				translate(v = [0, 0, -l * 0.3])
					cylinder(r1 = r * 1.1, r2 = r * 1.1, h = l * 0.2, center = true);
				}
			cylinder(r1 = 0.8 * r, r2 = 0.8 * r, h = l * 1.3, center = true);
			}
		}
}

module fuselage()
{
	union() {
	difference() {
		fuselage_bulk();
		translate(v = [-20, 0, 0])
			cube(size = [40, 20, 20], center = true);
		rotate(v = [0, 0, 1], 25)
			translate(v = [-27, 0, 0])
				cube(size = [10, 50, 50], center = true);
		rotate(v = [0, 0, 1], -25)
			translate(v = [-27, 0, 0])
				cube(size = [10, 50, 50], center = true);
		translate(v = [20, 10, -3.5])
			cube(size = [20, 2, 2], center = true);
		translate(v = [20, -10, -3.5])
			cube(size = [20, 2, 2], center = true);
		translate(v = [10, 17, 1.0])
			cube(size = [20, 2, 20], center = true);
		translate(v = [10, -17, 1.0])
			cube(size = [20, 2, 20], center = true);
		translate(v = [20, 10, 0.5])
			cube(size = [20, 7, 2], center = true);
		translate(v = [20, -10, 0.5])
			cube(size = [20, 7, 2], center = true);
		translate(v = [30, 0, -0.8])
			rotate(v = [0, 1, 0], a = 15)
				scale(v = [2, 1, 0.4])
					sphere(r = 3);
	}
	engine(-9, -4.5, 0, 3.5, 20);
	engine(-9, 4.5, 0, 3.5, 20);
	translate(v = [2, -9, 0])
		sphere(r = 5.5);
	translate(v = [2, 9, 0])
		sphere(r = 5.5);
	translate(v = [6, 0, 0])
		sphere(r = 6);
	translate(v = [30, 0, -1.9])
		rotate(v = [0, 1, 0], a = 15)
			scale(v = [2, 1, 0.6])
				sphere(r = 2.0);
	}
}

rotate(v = [1, 0, 0], a = -90)
	scale(v = [1.5, 1.5, 1.5])
		fuselage();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
        imposter_thrust(-33, 0, -4.5 * 1.5, 3.75);
        imposter_thrust(-33, 0, 4.5 * 1.5, 3.75);
}


