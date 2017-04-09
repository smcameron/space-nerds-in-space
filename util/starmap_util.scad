$fn=3;

module spike(r)
{
	cylinder(h = r, r1 = r * 0.1, r2 = 0, center = true);
}

module starspike(r)
{
	translate(v = [0, 0, r / 2])
		spike(r);
	translate(v = [0, 0, -r / 2])
		rotate(v = [1, 0, 0], a = 180)
			spike(r);
}

module star(x, y, z, r)
{
	translate(v = [x, y, z]) {
		starspike(r);
		rotate(v = [1, 0, 0], a = 90)
			starspike(r);
		rotate(v = [0, 1, 0], a = 90)
			starspike(r);
	}
}

module rod(a, b, r)
{
	hull() {
		translate(a) sphere(r);
		translate(b) sphere(r);
	}
}

