
$fn=16;

module engine()
{
	rotate(a = 90, v = [1, 0, 0]) {
		difference() {
			cylinder(h = 9, r1 = 0.75, r2 = 0.3, center = true);
			translate(v = [0, 0, 5.5])
				cylinder(h = 5, r1 = 0, r2 = 3,  center = true);
			translate(v = [0, 0, -5])
				cylinder(h = 5, r1 = 3, r2 = 0,  center = true);
		}
	}
}

module wedge()
{

	difference() {
		scale(v = [5, 10, 1.5]) {
			rotate(a = 45, v = [1, 0, 0])
				rotate(a = 45, v = [0, 1, 0])
					cube(size = 1, center = true);
		}
		translate(v = [0, 5, 0])
			cube(size = 10, center = true);
	}
}

module main_body()
{
	rotate(a = -90, v = [1, 0, 0])
	rotate(a = 90, v = [0, 0, 1])
	union() {
		wedge();
		translate(v = [2, -3, 0])
			engine();
		translate(v = [-2, -3, 0])
			engine();
	}
}

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-34, 0, 14, 2.0);
	imposter_thrust(-34, 0, -14, 2.0);
}

module neck()
{
	translate(v = [6.5, 0, 0])
	rotate(a = -90, v = [1, 0, 0])
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 10, r1 = 0.8, r2 = 0.5, center = true);
}

module cockpit()
{
	scale(v = [0.9, 0.5, 1.0])
	translate(v = [14, 0, 0])
	rotate(v = [0, 0, 1], a = 180)
	rotate(v = [0, 1, 0], a = 90)
	scale(v = [0.3, 0.3, 0.3])
	union() {
		sphere(r = 5, center = true);
		translate(v = [0, 0, 4.5])
			cylinder(h = 9, r1 = 4.9, r2 = 2, center = true);
	}
}

module conquerer()
{
	scale(v = [7, 7, 7]) {
		main_body();
		neck();
		cockpit();
	}
}

translate (v = [-30, 0, 0])
	conquerer();

