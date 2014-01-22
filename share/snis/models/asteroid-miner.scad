
$fn=16; 

module gripper()
{
	translate(v = [2.5, 0.3, -0.25])
		difference() {
			scale(v = [1, 1, 0.5])
				rotate(a = 45, v = [0, 1, 0])
					cube(size = [5, 0.6, 5], center = true);
			translate(v = [-6, 0, 0])
				cube(size = [5, 5, 5], center = true);
			translate(v = [2, 0, 0])
				cube(size = [5, 1.5, 1.5], center = true);
		}
}

module leg(angle)
{
	rotate(a = 180, v = [0, 1, ])
	union() {
		cube(size = [0.6, 0.6, 10], center = false);
		translate(v = [0, 0, 10]) {
			rotate(a = angle, v = [0, 1, 0])
				cube(size = [0.6, 0.6, 10], center = false);
			translate(v = [cos(90 - angle) * 10, 0, sin(90 - angle) * 10])
				rotate(a = -angle, v = [0, 1, 0])
					gripper();
		}
	}
}

module legset()
{
	scale(v = [0.7, 0.7, 0.7])
	rotate(a = 60, v = [0, 0, 1])
		translate(v = [3, 0, 0])
		rotate(a = -30, v = [0, 1, 0])
			leg(45);
	scale(v = [0.7, 0.7, 0.7])
	rotate(a = -60, v = [0, 0, 1])
		translate(v = [3, 0, 0])
		rotate(a = -35, v = [0, 1, 0])
			leg(25);
	rotate(a = 0, v = [0, 0, 1])
		translate(v = [3, 0, 0])
		rotate(a = 10, v = [0, 1, 0])
			leg(85);
}

module main_body()
{
	union() {
		difference() {
			translate(v = [0, 0, 5])
				scale(v = [1, 1, 0.3])
				sphere(r = 11);
			translate(v = [0, 0, -10.1])
				cube(size = [20, 20, 20], center = true);
		}
		translate(v = [0, 0, 1.2])
			cylinder(h = 4, r1 = 7.5, r2 = 7.5, center = true);
		translate(v = [3, 3, 2])
			cube(size = [0.4, 0.4, 15]);
			/* cylinder(h = 15, r1 = 0.4, r2 = 0.4); */
		translate(v = [-3, -3, 8])
			sphere(r = 3);
		translate(v = [-5, 0, -1.5])
			sphere(r = 3);
		translate(v = [5, 0, 2.5])
			cube(size = [10, 15, 5], center = true);
		translate(v = [8, 0, 2.5])
		rotate(a = 90, v = [0, 1, 0]) 
			difference() {
				cylinder(h = 8, r1 = 1, r2 = 2.3);
				cylinder(h = 8.02, r1 = 0, r2 = 2.2);
			}
		translate(v = [8, -5, 2.5])
		rotate(a = 90, v = [0, 1, 0]) 
			difference() {
				cylinder(h = 5, r1 = 1, r2 = 2.3);
				cylinder(h = 5.02, r1 = 0, r2 = 2.2);
			}
		translate(v = [8, 5, 2.5])
		rotate(a = 90, v = [0, 1, 0])
			difference() {
				cylinder(h = 5, r1 = 1, r2 = 2.3);
				cylinder(h = 5.02, r1 = 0, r2 = 2.2);
			}
	}
}

rotate (a = -90, v = [1, 0, 0])
rotate (a = 180, v = [0, 0, 1]) {
	main_body();
	translate(v = [-7, 0, 0])
		legset();
}

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-15, 2.5, 0, 1.4);
	imposter_thrust(-13, 2.5, 5, 1.4);
	imposter_thrust(-13, 2.5, -5, 1.4);
}

