



module first_wedge() {
	difference() {
		cube(size = [10,10,1], center = true);
		translate(v = [5, 0, 0.2]) {
			rotate(a = 10,  v = [0, 1, 0])
			cube(size = [11,11,1], center = true);
		}
	}
}

module second_wedge()
{
	translate(v = [-2, 0, 0]) {
		rotate(a = -45 / 2, v = [0, 0, 1]) {
			difference() {
				first_wedge();
				rotate(a = 45 / 2, v = [0, 0, 1]) {
					translate(v = [-2.70, 0, 0]) {
						cube(size = [11, 16, 5], center = true);
					}
				}
			}
		}
	}
}

module fin()
{
	scale(v = [0.2, 0.2, 0.2]) {
		rotate(a = -90, v = [0, 1, 0]) {
			second_wedge();
		}
	}

}

module third_wedge()
{
	difference() {
		union() {
			translate(v = [3.25, 2.5, -0.5]) 
				fin();
			second_wedge();
		}
		translate(v = [0, 5.7, 0])
			cube(size = [10, 5, 5], center = true);
		translate(v = [0, -8.3, 0])
			cube(size = [10, 5, 5], center = true);
	}
}

module fourth_wedge()
{
	difference() {
		third_wedge();
		translate(v = [0, 3.5, 0])
			cylinder(h = 5, r1 = 2, r2 = 2, center = true);
	}
}

module pod()
{
	union() {
		cylinder(h = 0.4, r1 = 2, r2 = 1, center = true);
		translate(v = [0, 0, -0.4])
			cylinder(h = 0.4, r1 = 1, r2 = 2, center = true);
	}
}

module fifth_wedge()
{
	union() {
		fourth_wedge();
		mirror(v = [1, 0, 0])
			fourth_wedge();
		translate(v = [0, 1.5, 0.20])
			pod();
	}
}

module bridge_buttress()
{
	rotate(a = 20, v = [1, 0, 0])
		cube(size = [0.15, 2.4, 0.35], center = true);
}

module bridge()
{
	rotate(a = 90, v = [1, 0, 0])
		scale(v = [1, 0.25, 1])
			cylinder(h = 1.5, r1 = 1.5, r2 = 1.1, center = true);
	
}

module sixth_wedge()
{
	union() {
		translate(v = [0, 3.4, 1.0])
			bridge();
		fifth_wedge();
		translate(v = [0.75, 2.85, 0.55])
			bridge_buttress();
		translate(v = [-0.75, 2.85, 0.55])
			bridge_buttress();
	}
}

$fn = 8;
rotate(a = 90, v = [0, 1, 0])
	rotate(a = -90, v = [1, 0, 0])
	scale (v = [2.4, 2.4, 4.8])
		sixth_wedge();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-8, 0, 0, 1.25);
}
