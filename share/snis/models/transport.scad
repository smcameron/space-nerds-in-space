
module transport()
{
	union() {
		translate(v = [0, 0, -2])
			scale(v = [1, 1, 1.5])
				rotate(a = 90, v = [0, 1, 0])
					cylinder(h = 38, r1 = 3.5, r2 = 3.5, center = true);
		translate(v = [20, 0, 0])
			cube(size = [20, 3, 3], center = true);
		translate(v = [28, 0, 0])
			scale(v = [1, 1, 1.5])
				sphere(r = 5);
		translate(v = [-15, 0, 8])
			scale(v = [1.8, 1, 1])
				sphere(r = 7);
			translate(v = [-25, 0, 2])
			scale(v = [1.8, 1, 1])
				sphere(r = 11);
		translate(v = [8, 0, 4])
			scale(v = [1.8, 1.5, 1])
				sphere(r = 5);
		translate(v = [-25, 0, 10])
			cylinder(h = 20, r1 = 2, r2 = 2, center = true);
		translate(v = [-25, 0, 19])
			scale(v = [1, 1.5, 0.3])
				sphere(r = 5);
	}
}

$fn = 5;

rotate(a = -90, v = [1, 0, 0])
	transport();
