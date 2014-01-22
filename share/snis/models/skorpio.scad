
$fn=12;

module segment(x, y, z, s)
{
	translate(v = [x, y, z])
		scale(v = [0.7 * s, 0.2 * s, 1.5 * s]) {
			difference() {
				sphere(r = 10);
					translate(v = [11, 0, 0])
						cylinder(h = 30, r1 = 10, r2 = 10, center = true);
			}
		}
}

module main_body() {
	rotate(a = 180, v = [0, 0, 1])
	rotate(a = 90, v = [0, 1, 0]) {
	union() {
		segment(0, 0, 0, 1);
		rotate(a = 20, v = [0, 1, 0])
			segment(3, 0, -2, 0.8);
		rotate(a = 30, v = [0, 1, 0])
			segment(5, 0, -2, 0.7);
		rotate(a = 0, v = [0, 1, 0])
			scale(v = [1, 4, 1])
				segment(0, 0, -7, 0.6);
	}
	}
}

rotate(a = -90, v = [1, 0, 0])
union() {
main_body();
rotate(a = 90, v = [0, 1, 0]) {
	difference() {
		cylinder(h = 35, r1 = 3, r2 = 0.2, center = true);
		translate(v = [0, 0, -17])
			cylinder(h = 6, r1 = 5, r2 = 0, center = true);
	}
}
/*
translate(v = [8, 0, 5.8])
rotate(a = 20, v = [0, 1, 0])
	scale(v = [1, 0.5, 0.5])
		sphere(r = 2);
*/
translate(v = [14, 0, -1.3])
	scale(v = [1.4, 1, 1])
	sphere(r = 1.5);
}

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-17, 0, 0, 2);
}

