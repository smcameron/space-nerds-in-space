
module fin()
{
	rotate(a = 45, v = [0, 0, 1])
		difference() {
			cube(size = [10, 10, 0.5], center = true);
			translate(v = [2, 2, 0])
				cube(size = [10, 10, 2], center = true);
		}
}

module fin_pair()
{
	union() {
		fin();
		rotate(a = 90, v = [0, 1, 0])
			fin();
	}
}

module outrigger()
{
	rotate(a = 90, v = [1, 0, 0])
		cylinder(h = 7, r1 = 1.5, r2 = 1.5, center = true);
}

module outrigger_set()
{
	translate(v = [7, 0, 0])
		outrigger();
	rotate(a = 90, v = [0, 1, 0])
		translate(v = [7, 0, 0])
			outrigger();
	rotate(a = 180, v = [0, 1, 0])
		translate(v = [7, 0, 0])
			outrigger();
	rotate(a = 270, v = [0, 1, 0])
		translate(v = [7, 0, 0])
			outrigger();
}
	
module fins_plus_outriggers()
{
	union() {
		outrigger_set();
		fin_pair();
	}
}

module main_ball()
{
	scale(v = [1, 1, 0.75])
		sphere(r = 8);
}

module fuselage()
{
	translate(v = [0, 10, 0])
		rotate(a = 90, v = [1, 0, 0])
			cylinder(h = 10, r1 = 2, r2 = 1, center = true);
}

module ball_plus_fuselage()
{
	union() {
		main_ball();
		fuselage();
	}
}

module little_pod()
{
	scale(v = [1, 1.5, 1])
		sphere(r = 1.5);
}

module ship()
{
	rotate(a = -90, v = [0, 0, 1])
	union() {
		ball_plus_fuselage();
		rotate(a = 45, v = [0, 1, 0])
			translate(v = [0, 17, 0])
				fins_plus_outriggers();
		translate(v = [5, -5, -3])
			little_pod();
	}
}

$fn = 8;
rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
ship();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-21, 5, 5, 1);
	imposter_thrust(-21, 5, -5, 1);
	imposter_thrust(-21, -5, 5, 1);
	imposter_thrust(-21, -5, -5, 1);
}

