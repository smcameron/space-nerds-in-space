

$fn=12; 

module engine()
{
	rotate(a = 180, v = [0, 1, 0])
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

module mainbody()
{
	scale(v = [0.4, 1, 1]) {
		cylinder(h = 14, r1 = 6, r2 = 1.5, center = true);
		translate(v = [0, 0, -8.5]) {
			sphere(r = 6.5, center = true);
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 2, r1 = 7.5, r2 = 7.5, center = true);
		}
	}
}

module pod()
{
	scale(v = [0.5, 0.5, 1])
		sphere(r = 3);
}

module outrigger()
{
	translate(v = [1, -5, -8])
		rotate(a = 25, v = [0, 0, 1])
			rotate(a = 70, v = [1, 0, 0])
				scale(v = [0.4, 1, 1])
					cylinder(h = 8, r1 = 1.5, r2 =0.7, center = true);
	translate(v = [2.5, -8, -7])
	pod();
}

module wombat()
{
	mainbody();

	translate(v = [0, 0, -15.5])
		scale(v = [1.2, 1.2, 1.2])
			engine();
	translate(v = [0, -2.5, -15.0])
		scale(v = [1.1, 1.1, 1.2])
			engine();
	translate(v = [0, 2.5, -15.0])
		scale(v = [1.1, 1.1, 1.2])
			engine();
	translate(v = [0, -5, -13.5])
		engine();
	translate(v = [0, 5, -13.5])
		engine();


	mirror(v = [0, 1, 0])
		outrigger();
	outrigger();

	translate(v = [1.3, 0, 3])
		scale(v = [0.37, 0.37, 1])
			sphere(r = 7.2);
}

translate(v = [12, 1, 0])
	rotate(a = -90, v = [1, 0, 0])
		rotate(a = 90, v = [0, 1, 0])
			scale(v = [1.4, 1.8, 1.8])
				wombat();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-22, 1, 0, 1);
	imposter_thrust(-20.25, 1, 4.5, 0.91);
	imposter_thrust(-20.25, 1, -4.5, 0.91);
	imposter_thrust(-16.25, 1, 9, 0.83);
	imposter_thrust(-16.25, 1, -9, 0.83);
}
