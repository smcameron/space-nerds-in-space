use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-13, 2, 0, 1.0);
	imposter_thrust(-13, -2, 0, 1.0);
	imposter_thrust(-13, 0, 2, 1.0);
	imposter_thrust(-13, 0, -2, 1.0);
}

module engine()
{
	rotate(a = -90, v = [0, 1, 0])
	scale(v = [0.15, 0.15, 0.15])
	difference() {
		union() {
			sphere(r = 6);
			translate(v = [0, 0, 4])
				cylinder(h = 10, r1 = 3, r2 = 6);
		}
		translate(v = [0, 0, 4])
			cylinder(h = 11, r1 = 2, r2 = 5.9);
	}
}

module escape_pod(r)
{
	scale(v = [0.4, 0.4, 0.4]) {
		rotate(v = [0, 1, 0], 90) {
			translate(v = [0, 0, r])
			cylinder(h = r , r1 = r, r2 = r / 3, center = true);
			translate(v = [0, 0, -r * 2])
			rotate(v = [1, 0, 0], a = 180)
				cylinder(h = r * 0.3, r1 = r, r2 = r / 2, center = true);
			translate(v = [0, 0, -r * 0.73])
				cylinder(h = r * 2.5, r1 = r, r2 = r, center = true);
		}
		translate(v = [-21, -5, 0])
			scale(v = [3, 3, 3])
				engine();
		translate(v = [-21, 5, 0])
			scale(v = [3, 3, 3])
				engine();
		translate(v = [-21, 0, 5])
			scale(v = [3, 3, 3])
				engine();
		translate(v = [-21, 0, -5])
			scale(v = [3, 3, 3])
				engine();
	}
}

escape_pod(10);

