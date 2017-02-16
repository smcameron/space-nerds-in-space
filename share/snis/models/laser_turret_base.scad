$fn = 16;

module turret_base_arm()
{
	union() {
		translate(v = [0, -29, 20])
			rotate(v = [1, 0, 0], a = 45)
				cube(size = [20, 35, 10], center = true);
		translate(v = [0, -40, 4])
				cube(size = [21, 10, 20], center = true);
		translate(v = [0, -20, 0])
			rotate(v = [1, 0, 0], a = 90)
				cylinder(h = 25, r1 = 3, r2 = 3);
	}
}

module turret_base()
{
	union() {
		translate(v = [0, 0, 30])
			cylinder(h = 20, r1 = 15, r2 = 25, center = true);
		translate(v = [0, 0, 50])
			cylinder(h = 20, r1 = 25, r2 = 25, center = true);
		turret_base_arm();
		rotate(v = [0, 0, 1], a = 180)
			turret_base_arm();
	}
}

rotate(v = [1, 0, 0], 90)
	turret_base();
