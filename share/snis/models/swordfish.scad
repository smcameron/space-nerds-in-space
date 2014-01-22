
$fn=12;


module engine()
{
	rotate(a = -90 - 45, v = [0, 0, 1])
	rotate(a = -90, v = [0, 1, 0])
	scale(v = [0.25, 0.25, 0.25])
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
	rotate(a = 45, v = [0, 0, 1])
		scale(v = [2, 0.8, 0.8])
			sphere(r = 2, center = true);
}

module body()
{
	scale(v = [1, 1, 0.7]) {
		difference() {
			union() {
				scale(v = [1, 1, 0.2])
					sphere(r = 10, center = true);
			}
			translate(v = [5, 5, 0])
				cube(size = [10, 10, 10], center = true);
		}
		scale(v = [1, 1, 0.55])
			sphere(r = 5, center = true);
	}
	translate(v = [3, 3, 0])
		scale(v = [1.2, 1.2, 1.2])
			engine();
	translate(v = [6, 0, 0])
		scale(v = [0.8, 0.8, 0.8])
			engine();
	translate(v = [0, 6, 0])
		scale(v = [0.8, 0.8, 0.8])
			engine();
	translate(v = [3, -8, 0])
		pod();
	translate(v = [-8, 3, 0])
		pod();
}

module divot()
{
	translate(v = [-6, -6, 0])
	rotate(a = -45, v = [0, 0, 1])
		scale(v = [0.8, 1, 1])
			cylinder(h = 9, r1 = 4, r2 = 4, center = true);
}

module swordfish()
{
	difference() {
		body();
		divot();
	}

	translate(v = [-5.5, -5.5, 0])
		rotate(a = 45, v = [0, 0, 1])
		scale(v = [1, 0.7, 0.3])
			sphere(r = 5, center = true);
}

scale(v = [2.4, 2.0, 2.0])
rotate(a = 90, v = [0, 0, 1])
rotate(a = 90, v = [0, 1, 0])
rotate(a = 45, v = [0, 0, 1])
	swordfish();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-20, 0, 0, 2.3);
	imposter_thrust(-16.75, 0, 8.5, 1.5);
	imposter_thrust(-16.75, 0, -8.5, 1.5);
}

