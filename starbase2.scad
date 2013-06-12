
$fn = 8;

scale(v = [5, 5, 5])
union() {
	difference() {
		scale(v = [1, 1, 0.15])
			sphere(r = 20);
		cylinder(h = 10, r1 = 15, r2 = 15, center = true);
	}
	cube(size = [35, 1, 1], center = true);
	rotate(a = 90, v = [0, 0, 1])
		cube(size = [35, 1, 1], center = true);
	scale(v = [0.3, 0.3, 2])
	sphere(r = 8, center = true);
	translate(v = [0, 0, -8]) {
		translate(v = [3, 0, 0])
			cylinder(h = 5, r1 = 2, r2 = 2, center = true);
		rotate(a = 360 / 3.0, v = [0, 0, 1])
			translate(v = [3, 0, 0])
				cylinder(h = 5, r1 = 2, r2 = 2, center = true);
		rotate(a = -360 / 3.0, v = [0, 0, 1])
			translate(v = [3, 0, 0])
				cylinder(h = 5, r1 = 2, r2 = 2, center = true);
	}
}

