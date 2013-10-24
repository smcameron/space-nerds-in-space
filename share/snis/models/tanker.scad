
module little_pod()
{
	scale(v = [1, 0.5, 0.5])
		sphere(r = 3.5);
}

module main_tank()
{
		union() {
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 24, r1 = 5, r2 = 5, center = true);
			translate(v = [12, 0, 0])
				scale(v = [3, 1.2, 1])
				sphere(r = 7.1);
			translate(v = [-12, 0, 0])
				sphere(r = 5.1);
			translate(v = [-14, 0, 7])
				little_pod();
			translate(v = [-13, 0, 5])
				rotate(a = -30, v = [0, 1, 0])
					cylinder(h = 8, r1 = 1, r2 = 0.2, center = true);
			translate(v = [20, -2.5, -5])
				rotate(a = -35, v = [1, 0, 0])
					cylinder(h = 14, r1 = 0.7, r2 = 6, center = true);
			translate(v = [20, -3.5, -11])
				scale(v = [1, 1.2, 0.3])
					sphere(r = 5);
		}
}


$fn = 8;

rotate(a = 180, v = [0, 1, 0])
rotate(a = 90, v = [1, 0, 0])
main_tank();

