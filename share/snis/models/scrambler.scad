
$fn=12;
module frame()
{	
	union() {
		translate(v = [0, -1.7, 0])
			rotate(a = 20, v = [0, 0, 1])
				rotate(a = 45, v = [1, 0, 0])
					cube(size = [10, 0.7, 0.7], center = true);
		translate(v = [0, 1.7, 0])
			rotate(a = -20, v = [0, 0, 1])
				rotate(a = 45, v = [1, 0, 0])
					cube(size = [10, 0.7, 0.7], center = true);
		rotate(a = 45, v = [1, 0, 0])
			cube(size = [10, 0.7, 0.7], center = true);
		translate(v = [-3, 0, 0])
			rotate(a = 90, v = [0, 0, 1])
				rotate(a = 45, v = [1, 0, 0])
					cube(size = [10, 0.7, 0.7], center = true);
	}
}

module engine()
{
	rotate(a = -90, v = [0, 1, 0])
	scale(v = [0.15, 0.15, 0.15])
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
	difference() {
		scale(v = [1.8, 0.35, 0.35])
			sphere(r = 3);
		translate(v = [4.5, 0, 0])
			scale(v = [3, 1, 1])
				sphere(r = 1);
		translate(v = [-4.5, 0, 0])
			scale(v = [3, 1, 1])
				sphere(r = 1);
	}
}

module scrambler()
{
	frame();
	translate(v = [-4.5, 0, 0])
		engine();
	translate(v = [-4.5, -3.3, 0])
		engine();
	translate(v = [-4.5, 3.3, 0])
		engine();
	translate(v = [-3.0, 5.3, 0])
		pod();
	translate(v = [-3.0, -5.3, 0])
		pod();

	scale(v = [1, 1, 0.7]) {
	translate(v = [5, 0, 0])
	rotate(a = 90, v = [0, 1, 0])
		cylinder(h = 2, r1 = 0.2, r2 = 2, center = true);
	translate(v = [8.3, 0, 0])
	rotate(a = 90, v = [0, 1, 0])
		cylinder(h = 5, r1 = 2, r2 = 2, center = true);
	translate(v = [10.3, 0, 0])
		scale(v = [2.5, 1, 1])
			sphere(r = 2.0);
	}
}

rotate(a = 90, v = [1, 0, 0])
	scale(v = [2.5, 2.5, 2.5])
		scrambler();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-16, 0, 0, 1.4);
	imposter_thrust(-16, 0, 8.25, 1.4);
	imposter_thrust(-16, 0, -8.25, 1.4);
}
