$fn = 16;

module barrel()
{
	union() {
		rotate(v = [0, 1, 0], a = 90) {
			translate(v = [0, 0, 25]) {
				difference() {
					cylinder(h = 100, r1 = 6, r2 = 4, center = true);
					translate(v = [0, 0, 25])
						cylinder(h = 100, r1 = 3, r2 = 3, center = true);
				}
				translate(v = [0, 0, -25])
					cylinder(h = 30, r1 = 8, r2 = 8, center = true);
			}
		}
	}
}

module barrel_pair()
{
	translate(v = [0, 20, 0])
		barrel();
	translate(v = [0, -20, 0])
		barrel();
}

module turret()
{
	union() {
		barrel_pair();
		translate(v = [-10, 0, 0]) {
		cube(size = [45, 20, 20], center = true);
			scale(v = [0.85, 0.3, 0.85])
				rotate(v = [1, 0, 0], a = 45)
					cube(size = [45, 20, 20], center = true);
			translate(v = [0, 5, 0])
				scale(v = [0.85, 0.3, 0.85])
					rotate(v = [1, 0, 0], a = 45)
						cube(size = [45, 20, 20], center = true);
			translate(v = [0, -5, 0])
				scale(v = [0.85, 0.3, 0.85])
					rotate(v = [1, 0, 0], a = 45)
						cube(size = [45, 20, 20], center = true);
		}
		cube(size = [15, 40, 8], center = true);
	}
}

rotate(v = [1, 0, 0], 90)
	turret();
