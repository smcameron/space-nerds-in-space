
$fn = 8;

module end() {
	scale(v = [4, 4, 4])
	union()
	{
		difference() {
			union() {
				cylinder(h = 5, r1 = 10, r2 = 10, center = true);
				translate(v = [0, 0, 3])
					scale(v = [1, 1, 0.15])
						sphere(r = 13);
			}
			cylinder(h = 20, r1 = 9, r2 = 9, center = true);
		}
	}
}

translate(v = [0, 0, -40])
	end();
translate(v = [0, 0, 40])
	rotate(a = 180, v = [0, 1, 0])
		end();
translate(v = [40, 0, 0])
	cube(size = [5, 5, 50], center = true);
rotate(a = 90, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);
rotate(a = -90, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);
rotate(a = 180, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);

scale(v = [0.1, 0.1, 1])
	sphere(r = 90);
cube(size = [5, 80, 5], center = true);
