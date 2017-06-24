


$fn=16;

module endpiece(r, h)
{
	difference() {
		cylinder(h = h, r1 = r, r2 = r, center = true);
		translate(v = [0, 0, 1.8 * h / 2])
			cylinder(h = h, r1 = r * 0.85, r2 = r * 0.85, center = true);
	}
}


module warp_core()
{
	rotate(a = 90, v = [0, 1, 0]) {
		union() {
			difference() {
				union() {
					translate(v = [0, 0, -15])
						rotate(v = [1, 0, 0], 180)
							endpiece(2, 3);
					translate(v = [0, 0, 15])
						endpiece(2, 3);
					cube(size = [0.3, 3.0, 30], center = true);
					cube(size = [3.0, 0.3, 30], center = true);
				}
				cylinder(h = 29, r1 = 1.3, r2 = 1.3, center = true);
			}
			cylinder(h = 29.5, r1 = 0.5, r2 = 0.5, center = true);
			translate(v = [0, 0, 10])
				cylinder(h = 3, r1 = 0.9, r2 = 0.9, center = true);
			translate(v = [0, 0, -10])
				cylinder(h = 3, r1 = 0.9, r2 = 0.9, center = true);
			translate(v = [0, 0, -5])
				cylinder(h = 1, r1 = 2, r2 = 2, center = true);
			translate(v = [0, 0, 5])
				cylinder(h = 1, r1 = 2, r2 = 2, center = true);

			translate(v = [0, 0, -3])
				cylinder(h = 0.3, r1 = 1.2, r2 = 1.2, center = true);
			translate(v = [0, 0, -2])
				cylinder(h = 0.3, r1 = 1.2, r2 = 1.2, center = true);
			translate(v = [0, 0, -1])
				cylinder(h = 0.3, r1 = 1.2, r2 = 1.2, center = true);
		}
	}
}

scale(v = [0.5, 0.5, 0.5]) {
	warp_core();
}


