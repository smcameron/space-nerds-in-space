module tentacle_segment(length, girth)
{
	union() {
		hull() {
			sphere(girth);
			translate(v = [length * 0.5, 0, 0])
				scale(v = [2.0, 1, 1])
					sphere(girth * 1.5);
		}
		hull() {
			translate(v = [length * 0.5, 0, 0])
				sphere(girth * 1.2);
			translate(v = [length, 0, 0])
				scale(v = [2.5, 1, 1])
				sphere(girth * 0.8);
		}
	}
}

module half_tentacle_segment()
{
	difference() {
		tentacle_segment(60, 6);
		translate(v = [-25, 0, -10])
			cube(size = [100, 20, 20]);
	}
}

translate(v = [0, -0.01, 0])
	half_tentacle_segment();
rotate(v = [1, 0, 0], a = 180)
	translate(v = [0, -0.01, 0])
		half_tentacle_segment();
