
module fuselage()
{	
	translate(v = [-2, 0, 0])
	rotate(a = 90, v = [0, 1, 0])
		cylinder(h = 24, r1 = 1.5, r2 =0.7, center = true);
	translate(v = [12, 0, 3])
	difference() {
		scale(v = [1, 0.3, 1])
			sphere(r = 5);
		translate(v = [2, 0, 0])
			scale(v = [1, 1, 1])
				sphere(r = 4);
	}
	translate(v = [-13, 0, 0])
		difference() {
			scale(v = [1, 1, 0.2])
				sphere(r = 9);
			translate(v = [-2, 0, 0])
			scale(v = [1, 1, 1])
				sphere(r = 8.5);
		}
	translate(v = [-13, 0, 2.0])
		scale(v = [2, 0.6, 1])
			sphere(r = 5);
	translate(v = [-11, 8, 0])
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 15, r1 = 1.2, r2 = 1.2, center = true);
	translate(v = [-11, -8, 0])
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 15, r1 = 1.2, r2 = 1.2, center = true);
	translate(v = [14, 0, 6])
		scale(v = [1.5, 1, 0.3])
			sphere(r = 3);
}

$fn=8;

union() {
	rotate(a = 180, v = [0, 0, 1])
		fuselage();
}
