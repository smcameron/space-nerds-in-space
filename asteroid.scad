
$fn=16;
rotate(a = 35, v = [1, 1, 0])
union() {
	sphere(r = 20);
	translate(v = [10, 10, 0])
		sphere(r = 16);
}

