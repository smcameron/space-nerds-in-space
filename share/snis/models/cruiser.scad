

module little_pod()
{
	scale(v = [1, 1.5, 1])
		sphere(r = 1.5, center = true);
}

module main_fuselage()
{
	union() {
		union() {
			rotate(a = 90, v = [1, 0, 0]) {
				scale(v = [1, 0.5, 1])
					cylinder(h = 20, r1 = 3.5, r2 = 1.8, center = true);
			}
			translate(v = [0, 5, 1.0])
				sphere(r = 2.5, center = true);
			translate(v = [0, -5, 0.8])
				sphere(r = 1.8, center = true);
		}
		translate(v = [0, 10, 0])
			sphere(r = 3.5, center = true);
		translate(v = [0, -10, 0])
			sphere(r = 1.8, center = true);
		translate(v = [4, 10, 0])
			little_pod();
		translate(v = [0, 5.5, 4])
			cylinder(h = 8, r1 = 0.1, r2 = 0.1, center = true);
		translate(v = [0, 5.1, 4])
			cylinder(h = 6, r1 = 0.1, r2 = 0.1, center = true);
	}
}

module sub_fuselage()
{
	union() {
		rotate(a = 90, v = [1, 0, 0]) {
			scale(v = [1, 1.5, 1])
				cylinder(h = 13, r1 = 0.7, r2 = 0.5, center = true);
		}
		translate(v = [0, 2, 1])
			scale(v = [1, 1, 1.5])
				sphere(r = 1.3, center = true);
	}
}

module engine(x, y, z, r)
{
	translate(v = [x, y, z]) {
		rotate(v = [1, 0, 0], a = 90)
		difference() {
			cylinder(h = r * 3.4, r1 = r, r2 = r, center = true);
			cylinder(h = r * 3.7, r1 = r * 0.85, r2 = r * 0.85, center = true);
		}
	}
}

module ship()
{
	rotate(a = -90, v = [0, 0, 1])
	union() {
		translate(v = [0, 0, -3.5])
			sub_fuselage();
		difference() {
			main_fuselage();
			translate(v = [0, 12.8, 0])
				cube(size = [5.5, 2.5, 2.9], center=true);
		}
		engine(0, 11.7, 0, 1.3);
		engine(2, 12.5, 0, 0.7);
		engine(-2, 12.5, 0, 0.7);
	}
}

$fn = 8;
rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
ship();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-15.0, 0, 0, 1.0);
	imposter_thrust(-14.3, 0, 2, 0.6);
	imposter_thrust(-14.3, 0, -2, 0.6);
}
