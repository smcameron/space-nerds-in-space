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

tentacle_segment(60, 6);

