$fn=15;

module caphole(angle)
{
	rotate(a = angle, v = [0, 0, 1])
		translate(v = [15, 0, 7.5])
			cylinder(h = 5, r1 = 3, r2 = 3, center = true);
}

module basetank(angle)
{
	rotate(a = angle, v = [0, 0, 1])
		translate(v = [5, 0, -56])
			cylinder(h = 9, r1 = 2.5, r2 = 2.5, center = true);
}

module antenna()
{
	translate(v = [0, -2, 20])
		cube(size = [0.3, 0.3, 10]);
	translate(v = [0, -3, 20])
		cube(size = [0.3, 0.3, 8]);
	translate(v = [0, -4, 20])
		cube(size = [0.3, 0.3, 17]);
	translate(v = [0, -5, 20])
		cube(size = [0.3, 0.3, 14]);
	translate(v = [0, 0, 20])
		rotate(a = 90, v = [1, 0, 0])
			cube(size = [0.3, 0.3, 5]);
}

module capgroove(angle)
{
	rotate(a = angle, v = [0, 0, 1])
		translate(v = [10, -4, 5.5])
			cube(size = [30, 8, 4], center = false);
}

module capball(angle)
{
	rotate(a = angle, v = [0, 0, 1])
		translate(v = [6, 0, -15])
			sphere(r = 4);
}

module mushroom_cap()
{
	union() {
		difference() {
			scale(v = [1, 1, 0.3])
				sphere(r = 30);
			cylinder(h = 50, r1 = 1, r2 = 6, center = true);
			translate(v = [0, 0, -4.6])
				cylinder(h = 6.5, r1 = 40, r2 = 40, center = true);
		}
		translate(v = [0, 0, -5])
			cylinder(h = 8, r1 = 15, r2 = 15, center = true);
		rotate(a = 180, v = [1, 0, 0]) {
			translate(v = [0, 0, 7]) {
				difference() {
					scale(v = [0.7, 0.7, 0.2])
						sphere(r = 30);
					/* cylinder(h = 50, r1 = 8, r2 = 8, center = true); */
					translate(v = [0, 0, -4.6])
						cylinder(h = 6.5, r1 = 40, r2 = 40, center = true);
				}
			}
		}
	}
}

module central_shaft()
{
	translate(v = [0, 0, -20])
		scale(v = [0.05, 0.05, 1])
			sphere(r = 50);
}

module capholes()
{
	caphole(0 * 360 / 5);
	caphole(1 * 360 / 5);
	caphole(2 * 360 / 5);
	caphole(3 * 360 / 5);
	caphole(4 * 360 / 5);
}

module capballs()
{
	capball(0 * 360 / 5);
	capball(1 * 360 / 5);
	capball(2 * 360 / 5);
	capball(3 * 360 / 5);
	capball(4 * 360 / 5);
}

module base_cyl()
{
		translate(v = [0, 0, -50]) {
			difference() {
			cylinder(h = 4, r1 = 9, r2 = 5, center = true);
			translate(v = [0, 0, -4.5])
				scale(v = [0.35, 0.35, 0.7])
					capgrooves();
			}
		}
}

module capgrooves()
{
	capgroove(0 * 360 / 5);
	capgroove(1 * 360 / 5);
	capgroove(2 * 360 / 5);
	capgroove(3 * 360 / 5);
	capgroove(4 * 360 / 5);
}

module basetanks()
{
	basetank(0 * 360 / 5);
	basetank(1 * 360 / 5);
	basetank(2 * 360 / 5);
	basetank(3 * 360 / 5);
	basetank(4 * 360 / 5);
}

module docking_port_connector(x, y, z, rx, ry, rz, angle)
{
	translate(v = [x, y, z])
		rotate(v = [rx, ry, rz], a = angle) {
			cylinder(h = 8, r1 = 1, r2 = 1, center = true);
			translate(v = [0, 0, -4])
				sphere(r = 1);
		}
}

scale(v = [3.0, 3.0, 3.0]) {
	union() {
		difference() {
			mushroom_cap();
			capholes();
			rotate(a = 360 / 10, v = [0, 0, 1])
			capgrooves();
		}
		rotate(a = 360 / 10, v = [0, 0, 1])
		scale(v = [0.5, 0.5, 0.65])
			capgrooves();
		capballs();
		base_cyl();
		basetanks();
		central_shaft();
		antenna();
		docking_port_connector(-62 / 3, -4 / 3, -27 / 3, 0, 1, 0, 65);
		docking_port_connector(62 / 3, 4 / 3, -27 / 3, 0, 1, 0, -65);
		docking_port_connector(4 / 3, -62 / 3, -27 / 3, 1, 0, 0, -65);
		docking_port_connector(-4 / 3, 62 / 3, -27 / 3, 1, 0, 0, 65);
	}
}

use <imposter_docking_port.scad>;
docking_ports = false;
if (docking_ports) {
	docking_port(-72, 0, -31, 0, 0, 1,-90, 0.2);
	docking_port(72, 0, -31, 0, 0, 1, 90, 0.2);
	docking_port(0, 72, -31, 0, 0, 1, 180, 0.2);
	docking_port(0, -72, -31, 0, 0, 1, 0, 0.2);
}

