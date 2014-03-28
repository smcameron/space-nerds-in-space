
$fn=16;

module tail()
{
	rotate(a = 90, v = [0, 1, 0])
	difference() {
		cylinder(h = 6, r1 = 15, r2 = 15, center = true);
		cylinder(h = 7, r1 = 14, r2 = 14, center = true);
	}
}

module tail_pillar()
{
	translate(v = [0, 0, -12])
	scale(v = [1, 0.2, 1])
	cylinder(h = 23, r1 = 2, r2 = 4, center = true);
}

module fuselage()
{
	scale(v = [1, 0.1, 0.1])
		sphere(r = 40);
}

module tail_assembly()
{
	translate(v = [20, 0, 0]) {
		tail();
		translate(v = [5, 0, 0])
		rotate(a = 20, v = [0, 1, 0])
			tail_pillar();
		translate(v = [-3, 0, -22])
		scale(v = [1, 0.3, 0.3])
			sphere(r = 5);
	}
}

module cockpit()
{
	translate(v = [-30, 0, 0]) {
		rotate(a = 20, v = [0, 1, 0])
			scale(v = [0.3, 0.3, 0.3])
				tail_pillar();
		translate(v = [-4, 0, -6])
			scale(v = [0.7, 0.3, 0.3])
				sphere(r = 5);
	}
}


module spacefarer()
{
	scale(v = [0.5, 0.5, 0.5]) {
		union() {
			tail_assembly();
			fuselage();
			cockpit();
		}
	}
}
rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
scale(v = [2, 2, 2])
	spacefarer();

