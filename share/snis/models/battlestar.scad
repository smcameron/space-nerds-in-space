
module battlestar()
{
	union() {
	translate(v = [5, 0, 0]) {
		cylinder(h = 40, r1 = 2, r2 = 0.5); 
		sphere(r = 2.5);
	}
	translate(v = [8, 0, -4]) {
		cylinder(h = 20, r1 = 3, r2 = 0.5); 
		sphere(r = 3.5);
	}
	translate(v = [13, 0, -9]) {
		cylinder(h = 10, r1 = 4, r2 = 0.5); 
		sphere(r = 4.5);
		scale(v = [0.2, 1.5, 0.5])
		sphere(r = 10);
	}
	}
}

$fn=8;

translate(v = [-8, 0, 0])
scale(v = [1.3, 1.3, 1.3])
rotate(a = 90, v = [1, 0, 0])
	rotate(a = 90, v = [0, 1, 0])
		translate(v = [-8, 0, 4])
			battlestar();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-12.5, 0, 0, 2);
}
