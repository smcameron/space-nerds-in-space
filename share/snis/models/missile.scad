$fn=16;

module missile()
{
	difference() {
		union() {
			cylinder(h = 30, r1 = 2, r2 = 2, center = true);
			translate(v = [0, 0, 14.99]) {
				scale(v = [1, 1, 5.9])
					sphere(r = 1.99);
			}
			translate(v = [0, 0, -15.5]) {
				cylinder(h = 0.5, r1 = 1.5, r2 = 2);
			}
			translate(v = [0, 0, -15])
				scale(0.95, 0.95, 0.95)
					finset();
		}
		translate(v = [0, 0, -15])
			scale(v = [0.25, 0.25, 1.0])
				sphere(r = 5);
	}
}

module fin()
{
	difference() {
		cube(size = [0.2, 10, 20]);
		union() {
			translate(v = [0, 10, 0])
				cube(size = [10, 10, 20], center = true);
			translate(v = [0, 15, 5])
				rotate(v = [1, 0, 0], a = 20)
					cube(size = [10, 20, 40], center = true);
		}
	}
}

module finset()
{
	for (i = [0:90:270])
		rotate(v = [0, 0, 1], i)
			fin();
}

rotate(v = [0, 1, 0], a = 90)
	missile();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-18, 0, 0, 1.6);
}

