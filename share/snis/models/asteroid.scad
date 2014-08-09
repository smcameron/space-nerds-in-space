
$fn=16;
rotate(a = 35, v = [1, 1, 0])
union() {
	scale(v = [1.3, 1.0, 1.0])
		sphere(r = 20);
	translate(v = [10, 10, 0])
		rotate(a = 70, v = [0, 1, 0])
			scale( v = [1.0, 1.2, 1.0])
				sphere(r = 16);
	rotate(a = 30, v = [0, 1, 1])
		translate(v = [0, -5, -15])
			rotate(a = 90, v = [0, 1, 0])
				scale(v = [1, 1.2, 1])
					sphere(r = 10);
}

