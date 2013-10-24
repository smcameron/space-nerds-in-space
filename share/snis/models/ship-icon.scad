
$fn = 8;

module pointer()
{
	scale(v = [0.5, 0.5, 1.5])
		cylinder(h = sqrt(2) * 5, r1 = 5, r2 = 0, center = true);
}

scale(v = [5, 5, 5])
	translate(v = [-1.7, 0, 0])
		rotate(a = 90, v = [0, 1, 0]) 
			pointer();

