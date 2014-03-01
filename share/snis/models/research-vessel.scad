$fn=16;

module square_segment()
{
	difference() {
		cube(size = [10, 10, 1], center = true);
		cube(size = [8, 8, 2], center = true);
	}
}

module upright_and_diagonal(angle)
{
	rotate(a = angle, v = [0, 0, 1]) {
		translate(v = [-4.5, -4.5, -4.5], center = true) {
			cube(size = [0.7, 0.7, 10], center = true);
			translate(v = [4.5, 0, 0], center = true)
				rotate(a = 45, v = [0, 1, 0])
					cube(size = [0.7, 0.7, 14.142], center = true);
		}
	}
}

module truss_segment()
{
	union() {
		square_segment();
		upright_and_diagonal(0);
		upright_and_diagonal(90);
		upright_and_diagonal(-90);
		upright_and_diagonal(180);
	}
}

module truss(length)
{
	rotate(a = 90, v = [0, 1, 0])
	for ( i = [0 : length]) {
		translate(v = [0, 0, i * 10])
			truss_segment();
	}
}

module nose_cone()
{
	intersection() {
		union() {
			translate(v = [-10, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 5, r1 = 10, r2 = 7, center = true);
			translate(v = [-17.5, 0, 0])
				rotate(a = 90, v = [0, 1, 0])
					cylinder(h = 10, r1 = 3, r2 = 10, center = true);
			translate(v = [-21.4, 0, 0])
				sphere(r = 3.2);
		}
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 60, r1 = 9.5, r2 = 9.5, center = true);
	}
}

module corner_thruster_a()
{
	rotate (a = 90, v = [0, 1, 0]) {
		union() {
			difference() {
				cylinder(h = 5, r1 = 2, r2 = 2.5, center = true);
				translate(v = [0, 0, 1])
					cylinder(h = 5, r1 = 2, r2 = 2.5, center = true);
			}
			translate(v = [0, 0, -4])
				sphere(r = 2.5);
		}
	}
}

module corner_thruster(angle)
{
	rotate(a = angle, v = [1, 0, 0])
		translate(v = [38, -5, -5])
			corner_thruster_a();
}

module tail()
{
		translate(v = [31.0, 0, 0]) {
			cube(size = [4, 15, 15], center = true);
			translate(v = [5, 0, 0])
			rotate(a = 90, v = [0, 1, 0]) {
				difference() {
					cylinder(h = 10, r1 = 3, r2 = 4, center = true);
					translate(v = [0, 0, 1])
					cylinder(h = 10, r1 = 3, r2 = 4, center = true);
				}
			}
		}
}

module fuel_tank()
{
	sphere(r = 5.5);
}

module whole_ship() {
	union()
	{
		nose_cone();
		truss(3);
		tail();
		fuel_tank();
		corner_thruster(0);
		corner_thruster(90);
		corner_thruster(-90);
		corner_thruster(180);
	}
}

module dish()
{
	translate(v = [-5, 35, -4.5])
	scale(v = [0.5, 0.5, 0.5]) {
		translate(v = [-10, 0, 0])
			cube(size = [20, 1, 1], center = true);
		translate(v = [-20, -35, 0])
			cube(size = [1, 69, 1], center = true);
		translate(v = [-20, 0, 0])
			rotate(v = [1, 0, 0], a = 74)
			cube(size = [1, 1, 64]);
		translate(v = [-20, 0, -0.5])
			rotate(v = [0, 0, 1], a = -20)
			rotate(v = [1, 0, 0], a = 90)
			cube(size = [1, 1, 65]);
		translate(v = [-20, 0, -0.5])
			cube(size = [4, 5, 5], center = true);
		difference() {
			sphere(r = 20);
			translate(v = [5.1, 0, 0]) {
				sphere(r = 23.5);
			}
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 26, r1 = 50, r2 = 50, center = true);
		}
	}
}

module science_module()
{
	translate(v = [41, 0, 0])
	rotate(v = [0, 1, 0], a = 90) {
		translate(v = [0, 0, 15])
		cylinder(h = 5, r1 = 12, r2 = 2, center = true);
		translate(v = [0, 0, -15])
		cylinder(h = 5, r1 = 2, r2 = 12, center = true);
		cylinder(h = 26, r1 = 12, r2 = 12, center = true);
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 26, r1 = 5, r2 = 5, center = true);
		rotate(a = 90, v = [1, 0, 0])
			cylinder(h = 26, r1 = 5, r2 = 5, center = true);
	}
}

rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
whole_ship();
dish();
science_module();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-47, 0, 0, 2.2);
	imposter_thrust(-44, 5, 5, 1.2);
	imposter_thrust(-44, 5, -5, 1.2);
	imposter_thrust(-44, -5, 5, 1.2);
	imposter_thrust(-44, -5, -5, 1.2);
}

