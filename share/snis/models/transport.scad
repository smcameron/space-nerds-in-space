
module half_cylinder()
{
	rotate(v = [0, 1, 0], a = 90)
		difference() {
			cylinder(h = 75, r1 = 15, r2 = 5, center = true);
			translate(v = [10, 0, 0])
			cube(size = [27, 35, 185], center = true);
		}
}

module engine(x, y, z, r)
{
	translate(v = [x, y, z]) {
		rotate(v = [0, 1, 0], a = 90)
			difference() {
				cylinder(h = 15, r1 = r, r2 = r, center = true);
				cylinder(h = 16, r1 = r * 0.8, r2 = r * 0.8, center = true);
			}
	}
}

module transport2()
{
	scale(v = [1, 0.5, 2])
	rotate(v = [1, 0, 0], a = 90) {
		translate(v = [0, 0, -2])
			half_cylinder();
		rotate(v = [1, 0, 0], 180)
			translate(v = [0, 0, -2])
				half_cylinder();
	}
	scale(v = [1, 0.7, 1])
	rotate(v = [0, 1, 0], a = 90) {
		scale(v = [1, 0.9, 1])
			cylinder(h = 90, r1 = 15, r2 = 2, center = true);
		translate(v = [15, 0, -8])
			cylinder(h = 70, r1 = 10, r2 = 2, center = true);
		translate(v = [-15, 0, -8])
			cylinder(h = 70, r1 = 10, r2 = 2, center = true);
	}
	scale(v = [1, 0.65, 1.4])
	translate(v = [-43, 0, 0]) {
		cylinder(h = 15, r1 = 15, r2 = 15, center = true);
		sphere(15);
		translate(v = [0, 0, -15]) {
			cylinder(h = 15, r1 = 8, r2 = 8, center = true);
			sphere(10);
		}
		translate(v = [0, 0, 15]) {
			cylinder(h = 15, r1 = 8, r2 = 8, center = true);
			sphere(10);
		}
	}
	translate(v = [-45, 0, 0])
	rotate(v = [1, 0, 0], a = 90)
		scale(v = [0.5, 1, 1])
			cylinder(h = 4, r1 = 31, r2 = 30, center = true);

	engine(-55, 0, 0, 3);
	engine(-55, 0, -7, 3);
	engine(-55, 0, 7, 3);

	translate(v = [-45, -9, 0])
	rotate(v = [0, 0, 1], a = 17)
		cube(size = [12, 5, 5], center = true);
	translate(v = [-45, -9, -7])
	rotate(v = [0, 0, 1], a = 17)
		cube(size = [12, 5, 5], center = true);
	translate(v = [-45, -9, 7])
	rotate(v = [0, 0, 1], a = 17)
		cube(size = [12, 5, 5], center = true);
}

$fn = 8;

rotate(a = 180, v = [1, 0, 0])
	transport2();


use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-65, 0, 0, 2);
	imposter_thrust(-65, 0, -7, 2);
	imposter_thrust(-65, 0, 7, 2);
}
