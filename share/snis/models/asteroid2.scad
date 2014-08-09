
$fn=16;
rotate(a = 27, v = [1, 0.25, -1]) 
union() {
	scale(v = [2, 1, 1])
		sphere(r = 20);
	rotate(a = 90, v = [1, 1, 1])
		translate(v = [4, 6, 2])
			scale(v = [1, 1.4, 1])
				sphere(r = 20);
	translate(v = [-9, -6, 2])
		scale(v = [1.4, 1.2, 1])
			sphere(r = 15);
}

