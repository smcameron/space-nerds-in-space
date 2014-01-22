

module trussrod()
{
		rotate(a = 80, v = [0, 1, 0])
			cube(size = [20, 0.5, 0.5], center = false);
}

module truss()
{
	union() {
		trussrod();
		rotate(a = 90, v = [0, 0, 1])
			trussrod();
		rotate(a = 180, v = [0, 0, 1])
			trussrod();
		rotate(a = -90, v = [0, 0, 1])
			trussrod();
	}
}

module main_fuselage()
{
	union() {
		rotate(a = 90, v = [0, 1, 0])
			translate(v = [0, 0, 23])
				truss();
		scale(v = [1.3, 1.3, 0.7])
			sphere(r = 7);
		translate(v = [-3, 0, -5])
			sphere(r = 3);
		translate(v = [22, 0, 0])
			cube(size = [3, 17, 1], center = true);
		translate(v = [25, 10, 0])
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 15, r1 = 2, r2 = 1.5, center = true);
		translate(v = [25, -10, 0]) 
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 15, r1 = 2, r2 = 1.5, center = true);
		translate(v = [17, -10, 0])
			sphere(r = 2);
		translate(v = [17, 10, 0])
			sphere(r = 2);
	}
}


rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
main_fuselage();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-33, 0, 10, 1.25);
	imposter_thrust(-33, 0, -10, 1.25);
}

