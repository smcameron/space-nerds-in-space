
$fn=16;

rotate(a = 27, v = [0.3, 0.7, 0.2])
union() {
scale(v = [2.3, 1, 1])
	sphere(r = 20);
translate(v = [5, 5, 10])
	scale(v = [1.3, 0.8, 1])
		sphere(r = 25);
}

