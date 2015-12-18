$fn=16;
module warpgate(r)
{
	difference() {
		cylinder(h = r * 0.5, r1 = r, r2 = r, center = true);
		cylinder(h = r * 0.7, r1 = r * 0.85, r2 = r * 0.85, center = true);
	}
}

module cuboid(angle, r, thickness) {
	rotate(v = [1, 0, 0], a = angle)
		translate(v = [0, 0, r]) {
			cube(size = [28, 15 * thickness, 30], center = true);
			translate(v = [0, 0, -0.15 * r])
				sphere(r = 7 * thickness);
			translate(v = [0, 0, 0.15 * r])
				sphere(r = 7 * thickness);
		}
}

module cuboid_quad(angle, r, t)
{
	cuboid(0 + angle, r, t);
	cuboid(90 + angle, r, t);
	cuboid(270 + angle, r, t);
	cuboid(180 + angle, r, t);
}

union() {
	rotate(v = [0, 1, 0], a = 90)
		warpgate(100);

	cuboid_quad(0, 85, 1.2);
	cuboid_quad(45, 85, 1.2);
	cuboid_quad(22.5, 92, 1.8);
	cuboid_quad(-22.5, 92, 1.8);
}

