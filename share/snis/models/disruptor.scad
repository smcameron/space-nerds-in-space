
$fn=8;

module main_body()
{
	scale(v = [1, 1.2, 0.7])
	rotate(a = 90, v = [0, 1, 0])  {
		difference() {
			union() {
				cylinder(h = 20, r1 = 1.7, r2 = 2.2, center = true);
				translate(v = [0, 0, 4])
					cylinder(h = 3, r1 = 2.3, r2 = 2.3, center = true);
			}
		translate(v = [0, 0, 10])
			cylinder(h = 3, r1 = 1.5, r2 = 2.1, center = true);
		translate(v = [0, 0, -10])
			cylinder(h = 3, r1 = 1.8, r2 = 1.0, center = true);
		}
	}
}

module top_tower()
{
	scale(v = [0.8, 0.3, 1.2]) {
		cylinder(h = 5, r1 = 3, r2 = 2, center = true);
		translate(v = [0, 0, 1.8])
			cylinder(h = 0.8, r1 = 2.5, r2 = 2.5, center = true);
	}
}

module bottom_tower()
{
	scale(v = [0.6, 0.2, 1.7]) {
		cylinder(h = 5, r1 = 3, r2 = 1, center = true);
		translate(v = [0, 0, 1.8])
			cylinder(h = 0.4, r1 = 1.9, r2 = 1.5, center = true);
	}
}

module bottom_pod()
{
	translate(v = [1.3, 0, -9.5]) {
		difference() {
		scale(v = [2.5, 1.3, 1])
			sphere(r = 1.0);
		scale(v = [1, 1.5, 1])
		translate(v = [-2.3, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 2.5, r1 = 0.5, r2 = 0.5, center = true);
		translate(v = [2.0, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 2.5, r1 = 0.5, r2 = 2.5, center = true);
		}
	}
}

module top_pod()
{
	translate(v = [4.2, 0, 5.6])
	rotate(a = 90, v = [0, 1, 0])
	scale(v = [0.5, 2, 1]) {
		difference() {
			cylinder(h = 5, r1 = 1.7, r2 = 2, center = true);
			translate(v = [0, 0, -3])
				cylinder(h = 2, r1 = 1.4, r2 = 1.4, center = true);
			translate(v = [0, 0, 3])
				cylinder(h = 2, r1 = 1.7, r2 = 1.7, center = true);
		}
	}
}

module antenna()
{
	translate(v = [0, 0, -5])
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 5, r1 = 0.2, r2 = 0.2, cetner = true);
	translate(v = [-3, 0, -6])
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 8, r1 = 0.2, r2 = 0.2, cetner = true);
}

rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
scale(v = [2.5, 2.5, 2.5])
union() {
	main_body();
	translate(v = [3.5, 0, 2])
		rotate(a = 10, v = [0, 1, 0])
			top_tower();
	translate(v = [2.0, 0, -5])
		rotate(a = 190, v = [0, 1, 0])
			bottom_tower();
	bottom_pod();
	top_pod();
	antenna();
}
use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-30, 0, 0, 2.0);
}

