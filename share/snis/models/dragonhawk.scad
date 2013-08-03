
$fn=12;
union() {
difference() {
	scale (v = [2, 0.5, 1])
		sphere(r = 10);
	translate(v = [10, 0, -48])
		rotate(a = 90, v = [1, 0, 0])
			cylinder(h = 10, r1 = 50, r2 = 50, center = true);
}
translate(v = [-0.5, 0, -3])
	rotate(a = 110, v = [0, 1, 0])
		cylinder(h = 14, r1 = 1, r2 = 3.3, center = true);

translate(v = [8, 0, -4])
scale(v = [1, 0.5, 0.5])
	sphere(r = 10);

translate(v = [0, 0, 3]) { 
translate(v = [12, 0, -5])
	scale(v = [1, 0.8, 0.2])
		sphere(r = 10);
translate(v = [12, 0, 5])
	cylinder(h = 10, r1 = 0.4, r2 = 0.4, center = true);
translate(v = [11, 0, 10])
	cylinder(h = 15, r1 = 0.3, r2 = 0.2, center = true);
translate(v = [10, 0, 7])
	cylinder(h = 8, r1 = 0.3, r2 = 0.3, center = true);
translate(v = [20, 5, -5])
	rotate(a = 90, v = [0, 1, 0])
		cylinder(h = 7, r1 = 0.9, r2 = 1.9, center = true);
translate(v = [20, -5, -5])
	rotate(a = 90, v = [0, 1, 0])
		cylinder(h = 7, r1 = 0.9, r2 = 1.9, center = true);
}
}

