
module little_pod()
{
	scale(v = [1, 0.5, 0.5])
		sphere(r = 3.5);
}

module tank(r, l)
{
	union() {
		cylinder(r = r, h = l - 1.95 * r, center = true);
		translate(v = [0, 0, (l - 1.95 * r ) / 2])
			sphere(r = r * 1.02);
		translate(v = [0, 0, -(l - 1.95 * r) / 2])
			sphere(r = r * 1.02);
	}
}

module ring(r, h)
{
	difference() {
		cylinder(h = h, r = r, center = true);
		cylinder(h = h * 2, r = r * 0.95, center = true);
	}
}

module rotated_translated_tank(x, y, z, a, r, l)
{

	rotate(a = a, v = [1, 0, 0])
		rotate(a = 90, v = [0, 1, 0])
			translate(v = [x, y, z])
				tank(r, l);
}

module engine()
{
	rotate(a = 90, v = [0, 1, 0])
	scale(v = [0.5, 0.5, 0.5])
	difference() {
		union() {
			sphere(r = 6);
			translate(v = [0, 0, 4])
				cylinder(h = 10, r1 = 3, r2 = 6);
		}
		translate(v = [0, 0, 4])
			cylinder(h = 11, r1 = 2, r2 = 5.9);
	}
}

module pod()
{
	translate(v = [23, 0, 10])
		rotate(a = -30, v = [0, 1, 0])
			scale(v = [1, 0.3, 0.5]) {
				difference() {
				cylinder(h = 120, r1 = 4, r2 = 2, center = true);
				rotate(a = 90, v = [1, 0, 0])
					scale(v = [1.8, 40, 10])
						cylinder(h = 1, r = 1, center = true);
				}
			}
	translate(v = [10, 0, 35])
	scale(v = [1, 1, 0.5])
		rotate(v = [0, 1, 0], a = 90) {
			difference() {
				tank(7, 19);
				translate(v = [0, 0, -10])
				scale(v = [0.8, 1, 0.8])
					sphere(r = 7);
			}
		}
	translate(v = [38, 0, -15])
		rotate(v = [0, 1, 0], a = 90)
			tank(5, 19);
	translate(v = [47, 0, -15])
		engine();
}

module antennae()
{
	cube(size = [0.5, 0.5, 20]);
	translate(v = [1, 0, 0])
		cube(size = [0.5, 0.5, 15]);
	translate(v = [-1.5, 0, 0])
		cube(size = [0.5, 0.5, 10]);
}

$fn = 16; 

rotate(v = [0, 1, 0], a = 180)
union() {
	translate(v = [-10, 0, 0]) {
		rotated_translated_tank(0, 0, 0, 0, 3.6, 40);
		rotated_translated_tank(7, 0, 0, 0,   3.6, 40);
		rotated_translated_tank(7, 0, 0, 60,  3.6, 40);
		rotated_translated_tank(7, 0, 0, 120, 3.6, 40);
		rotated_translated_tank(7, 0, 0, 180, 3.6, 40);
		rotated_translated_tank(7, 0, 0, 240, 3.6, 40);
		rotated_translated_tank(7, 0, 0, 300, 3.6, 40);
		translate(v = [-15, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				ring(11.0, 1);
		translate(v = [15, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				ring(11.0, 1);
		rotate(a = 90, v = [0, 1, 0])
				ring(11.0, 1);
	}
	translate(v = [20, 0, 0])
	rotate(a = 90, v = [0, 1, 0]) {
		cylinder(h = 20, r = 1, center = true);
	translate(v = [0, 0, 10])
		/* cylinder(h = 10, r1 = 0, r2 = 4, center = true); */
		scale(v = [0.3, 0.3, 1.0])
			sphere(r = 10);
	}
	pod();
	rotate(v = [1, 0, 0], 120)
		pod();
	rotate(v = [1, 0, 0], -120)
		pod();
	translate(v = [15, 0, 35])
		antennae();
}

use <imposter_thrust.scad>;

thrust = 0;
if (thrust) {
	imposter_thrust(-55, 13, -7.5, 2);
	imposter_thrust(-55, -13, -7.5, 2);
	imposter_thrust(-55, 0, 15, 2);
}

