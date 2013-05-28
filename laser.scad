
module torpedo()
{
	union() {
		cylinder(h = 10, r1 = 0.1, r2 = 0.1, center = true);
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 10, r1 = 0.1, r2 = 0.1, center = true);
		rotate(a = 90, v = [1, 0, 0])
			cylinder(h = 10, r1 = 0.1, r2 = 0.1, center = true);
	}
}

$fn = 3;
torpedo();
