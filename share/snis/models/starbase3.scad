
$fn = 16;

module end() {
	scale(v = [4, 4, 4])
	union()
	{
		difference() {
			union() {
				cylinder(h = 5, r1 = 10, r2 = 10, center = true);
				translate(v = [0, 0, 3])
					scale(v = [1, 1, 0.15])
						sphere(r = 13);
			}
			cylinder(h = 20, r1 = 9, r2 = 9, center = true);
		}
	}
}

translate(v = [0, 0, -40])
	end();
translate(v = [0, 0, 40])
	rotate(a = 180, v = [0, 1, 0])
		end();
translate(v = [40, 0, 0])
	cube(size = [5, 5, 50], center = true);
rotate(a = 90, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);
rotate(a = -90, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);
rotate(a = 180, v = [0, 0, 1])
	translate(v = [40, 0, 0])
		cube(size = [5, 5, 50], center = true);

scale(v = [0.1, 0.1, 1])
	sphere(r = 90);
cube(size = [5, 180, 5], center = true);

module pod()
{
	cylinder(h = 60, r1 = 10, r2 = 10, center = true);
	translate(v = [0, 0, 30])
		sphere(r = 10);
	translate(v = [0, 0, -30])
		sphere(r = 10);
}

translate(v = [0, 90, 0])
	rotate(v = [0, 1, 0], a = 90)
		pod();

translate(v = [0, -90, 0])
	rotate(v = [0, 1, 0], a = 90)
		pod();


use <imposter_docking_port.scad>;
docking_ports = false;
if (docking_ports) {
	docking_port2( 50,  90, 0, 0, 1, 0,  180, 0.3);
	docking_port2(-50, -90, 0, 0, 1, 0,  0, 0.3);
}

