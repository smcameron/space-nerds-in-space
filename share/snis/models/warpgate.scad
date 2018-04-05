$fn=16;

module torus(r1, r2)
{
	rotate(v = [0, 1, 0], 90) {
		rotate_extrude(convexity = 30)
		translate([r1, 0, 0])
			circle(r = r2, $fn = 100);
	}
}

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
	/* Note, the game hard codes the idea that the warp gate is more or less toroidal,
	 * and does collision detection based on this assumption. So if you change the model,
	 * it still needs to be more or less toroidal in the y,z plane with the x-axis passing
	 * through the donut hole. If you change the dimensions of the warp gate, you also need
	 * to change the values for WARPGATE_MAJOR_RADIUS and WARPGATE_MINOR_RADIUS in snis.h
	 * to match. See below on how to check the torus size.
	 */
	rotate(v = [0, 1, 0], a = 90)
		warpgate(100);

	cuboid_quad(0, 85, 1.2);
	cuboid_quad(45, 85, 1.2);
	cuboid_quad(22.5, 92, 1.8);
	cuboid_quad(-22.5, 92, 1.8);
}

check_torus_size = 0; // Change this to 1 to check the size of the collider torus.
major_radius = 92;
minor_radius = 25;
if (check_torus_size) {
		torus(major_radius, minor_radius);
}

