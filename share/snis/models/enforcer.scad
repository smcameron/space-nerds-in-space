
$fn=32;

module side_pod()
{
	cylinder(h = 20, r1 = 1, r2 = 1, center = true);
}

module wing_surface()
{
	rotate(a = 90, v = [0, 1, 0])
		scale(v = [1, 0.15, 1])
			cylinder(h = 20, r1 = 5, r2 = 10, center = true);
}

module wing() {
	translate(v = [-20, 0, 0]) {
		side_pod();
		translate(v = [10, 0, 3])
			wing_surface();
	}
}

module wingset() {
	wing();
	rotate(a = 360.0 / 3.0, v = [0, 0, 1])
		wing();
	rotate(a = 2.0 * 360.0 / 3.0, v = [0, 0, 1])
		wing();
}

module engine() {
	scale(v = [1, 1, 1.5])
	difference() {
		union() {
			translate(v = [0, 0, 2])
				cylinder(h = 1, r1 = 3.5, r2 = 3.5, center = true);
			translate(v = [0, 0, 4])
				cylinder(h = 1, r1 = 3.5, r2 = 3.5, center = true);
			cylinder(h = 15, r1 = 3, r2 = 3, center = true);
		}
		translate(v = [0, 0, 6])
			cylinder(h = 5, r1 = 1.0, r2 = 2.8, center = true);
		translate(v = [0, 0, -6])
			cylinder(h = 5, r1 = 2.8, r2 = 1.0, center = true);
	}
}

module engine_set() {
	translate(v = [9, 0, 0])
		engine();
	rotate(a = 360.0 / 3.0, v = [0, 0, 1])
		translate(v = [9, 0, 0])
			engine();
	rotate(a = 2.0 * 360.0 / 3.0, v = [0, 0, 1])
		translate(v = [9, 0, 0])
			engine();
}

module main_ship() {
	rotate(a = 90, v = [1, 0, 0])
	rotate(a = -90, v = [0, 1, 0])
	union() {
		wingset();
		rotate(a = 360.0 / 6.0, v = [0, 0, 1])
			translate(v = [0, 0, 4])
				scale(v = [0.7, 0.7, 0.5])
					wingset();
		translate(v = [0, 0, -17])
			scale(v = [0.3, 0.3, 3])
				sphere(r = 15);
		translate(v = [0, 0, 5])
		engine_set();
		translate(v = [2, 0, -48])
			scale(v = [0.1, 0.6, 0.9])
				difference() {
					sphere(r = 20);
					translate(v = [0, 0, 25])
						rotate(a = 90, v = [0, 1, 0])
						cylinder(h = 40, r1 = 25, r2 = 25, center = true);
				}
	}
}

main_ship();

use <imposter_thrust.scad>;

thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-18, -9, 0, 1.5);
	imposter_thrust(-18, 4.6, 7.9, 1.5);
	imposter_thrust(-18, 4.6, -7.9, 1.5);
}

