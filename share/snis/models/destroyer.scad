$fn = 8;

module flattened_cylinder(rad1, rad2, height, flatness)
{
	scale(v = [1, 1 / flatness, 1])
		cylinder(h = height , r1 = rad1, r2 = rad2, center = true);
	
}

module flattened_sphere(radius, flatness)
{
	scale(v = [1, 1, 1 / flatness])
		sphere(r = radius);
}

module long_sphere(radius, longness)
{
	scale(v = [1, 1 / longness, 1 / longness])
		sphere(r = radius);
}

module flat_cyl(height, rad1, rad2, flatness)
{
	scale(v = [1, 1 / flatness, 1])
		cylinder(h = height, r1 = rad1, r2 = rad2, center = true);
}

module fuselage(sz) {
	scale(v = [1.5, 1, 1])
	difference() {
		scale(v = [2, 1, 1])
			flattened_sphere(sz, 3);
	translate(v = [-sz * 1.5, 0, 0])
		scale(v = [2, 1, 1])
			cylinder(h = sz, r1 = sz * 0.8, r2 = sz * 0.8, center = true);
	translate(v = [sz * 2, 0, 0])
		scale(v = [2, 1, 1])
			cylinder(h = sz, r1 = sz * 0.5, r2 = sz * 0.5, center = true);
	}
}


rotate(a = -90, v = [1, 0, 0])
union() {
fuselage(10);
rotate(a = -30, v = [0, 1, 0])
translate(v = [-5, -7.5, 9])
flat_cyl(12, 3, 1, 4);
translate(v = [-12, -7, 10])
long_sphere(4, 1.5);

translate(v = [9, 8, -1.5])
rotate(a = 90, v = [0, 1, 0])
cylinder(h = 14, r1 = 0.1, r2 = 1.8, center = true);
translate(v = [15, 8, -1.5])
long_sphere(3.6, 1.5);

translate(v = [0, 0, -1])
rotate(a = 90, v = [0, 1, 0])
difference() {
	cylinder(h = 14, r1 = 3, r2 = 2, center = true);
	translate(v = [0, 0, -8])	
	cylinder(h = 15, r1 = 5.0, r2 = 0.5, center = true);
}

translate(v = [12, -5, 2.1])
long_sphere(6, 1.5);
}

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-6, -1, 0, 1.75);
}
