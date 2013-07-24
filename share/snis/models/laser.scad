
torpedo_radius = 10;
torpedo_fatness = 1.5;

module tetrahedron(radius, height)
{
	cylinder(h = height, r1 = radius, r2 = 0, center = false);
}

module spike()
{
	union() {
		tetrahedron(torpedo_fatness, torpedo_radius);
		mirror([0, 0, 1])
			tetrahedron(torpedo_fatness, torpedo_radius);
	}
}


module torpedo1()
{
	union() {
		spike();
		rotate(a = 90, v = [0, 1, 0])
			spike();
		rotate(a = 90, v = [1, 0, 0])
			spike();
	}
}

module torpedo()
{
	union() {
		torpedo1();
		rotate(a = 45, v = [1, 0, 0])
			rotate(a = 45, v = [0, 1, 1])
					torpedo1();
	}
}

$fn = 3;
torpedo1();
