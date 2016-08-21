
$fn=16;

module engine()
{
	rotate(v = [0, 1, 0], a = -90)
	difference() {
		union() {
			sphere(r = 6);
			translate(v = [0, 0, 4])
				cylinder(h = 10, r1 = 3, r2 = 6);
		}
		translate(v = [0, 0, 4])
			cylinder(h = 11, r1 = 2, r2 = 5.9);
	}
}

module engine_set()
{
	engine();
	translate(v = [0, 15, 0])
		engine();
	translate(v = [0, -15, 0])
		engine();

}

module fuselage_chamfer()
{
	rotate(v = [1, 0, 0], 45)
		cube(size = [200, 10, 10], center = true);
}

module fuselage_chamfer_pair()
{
	translate(v = [0, 26, 0])
		fuselage_chamfer();
	translate(v = [0, -26, 0])
		fuselage_chamfer();
}

module fuselage_chamfer_set()
{
	translate(v = [0, 0, 12])
		fuselage_chamfer_pair();
	translate(v = [0, 0, -12])
		fuselage_chamfer_pair();
}

module fuselage_base()
{
	difference() {
		cube(size = [100, 50, 20], center = true);
		fuselage_chamfer_set();
	}
}

module fuselage()
{
	difference() {
		fuselage_base();
		translate(v = [53, 0, 0])
			scale(v = [0.1, 0.9, 0.9])
				fuselage_base();
		translate(v = [-53, 0, 0])
			scale(v = [0.1, 0.9, 0.9])
				fuselage_base();
	}
}

module pontoon()
{
	rotate(v = [1, 0, 0], a = 90)
		scale(v = [1.1, 0.20, 0.5])
			fuselage();
}

module pontoon_pair()
{
	translate(v = [0, 60, -30])
		pontoon();
	translate(v = [0, -60, -30])
		pontoon();
}

module arm()
{
	rotate(v = [1, 0, 0], 15)
	rotate(v = [0, 1, 0], a = -90)
	intersection() {
		difference() {
		   cylinder(r1 = 40, r2 = 40, h = 10, center = true);
			translate(v = [-2, 5, 0])
		   cylinder(r1 = 33, r2 = 33, h = 20, center = true);
		}
		translate(v = [20, 20, 0])
			cube(size = [40, 40, 40], center = true);
	}
}

module armpair()
{
	translate(v = [0, 20, 0])
		arm();
	rotate(v = [0, 0, 1], a= 180)
		translate(v = [0, 20, 0])
			arm();
}

module armset()
{
	armpair();
	translate(v = [35, 0, 0])
		armpair();
	translate(v = [-35, 0, 0])
		armpair();
}

module fuselage_with_arms()
{
	fuselage();
	translate(v = [0, 0, -38])
		armset();
}

module robot_arm_segment(length)
{
	cube(size = [length, 2, 2], center = true);
}

module robot_joint(radius)
{
	sphere(r = radius * 0.9);
}

module robot_finger()
{
	union() {
		rotate(v = [0, 1, 0], a = 115) {
			translate(v = [2.5, 0, 0]) {
				cube(size = [3, 1, 1], center = true);
				translate(v = [2.5, 0, -0.7])
					rotate(v = [0, 1, 0], a = 30)
						cube(size = [3, 0.8, 0.8], center = true);
				translate(v = [3.7, 0, -2.6])
				rotate(v = [0, 1, 0], a = 90)
					cube(size = [3, 0.6, 0.6], center = true);
			}
		}
	}
}

module robot_finger_set()
{
	scale(v = [1.5, 1.5, 1.5]) {
		robot_finger();
		rotate(v = [1, 0, 0], a = 120)
			robot_finger();
		rotate(v = [1, 0, 0], a = -120)
			robot_finger();
	}
}

module robot_arm()
{
	union() {
		translate(v = [0, 0, -5])
			cylinder(r1 = 5, r2 = 2, h = 5, center = true);
		robot_joint(4);
		translate(v = [33, 0, 0])
			robot_arm_segment(70);
		translate(v = [70, 0, 0])
			robot_joint(4);
		translate(v = [40, 0, 12])
			rotate(v = [0, 1, 0], a = 20)
				robot_arm_segment(70);
		translate(v = [5, 0, 25])
			robot_joint(4);
		translate(v = [0, 0, 25])
			scale(v = [0.2, 0.7, 0.7])
				robot_arm_segment(70);
		translate(v = [-5, 0, 25])
			robot_joint(3);
		translate(v = [-5, 0, 25])
			robot_finger_set();
	}
}

module bridge()
{
	union() {
		translate(v = [0, 0, -5])
			rotate(v = [0, 1, 0], a = 90)
				cylinder(r1 = 3, r2 = 5, h = 100);
		sphere(r = 9);
		translate(v = [100, 0, -5])
			scale(v = [2.0, 1.0, 1.0])
				sphere(r = 5);
		//cylinder(r1 = 15, r2 = 3, h = 3, center = true);
		translate(v = [0, 0, -5.5])
			cylinder(r1 = 7.5, r2 = 7.5, h = 11, center = true);
		translate(v = [0, 0, -12])
			cylinder(r1 = 3, r2 = 10, h = 13, center = true);
	}
}

module tow_ship()
{
	union() {
		fuselage_with_arms();
		pontoon_pair();
		translate(v = [-50, 0, -10])
			engine_set();
		rotate(v = [1, 0, 0], a = 180)
			translate(v = [40, 0, 15])
				robot_arm();
		translate(v = [140, 0, 15])
			rotate(v = [0, 0, 1], a = 180)
				bridge();
	}
}

rotate (v = [1, 0, 0], a = -90)
	tow_ship();

use <imposter_thrust.scad>;

thrust_ports = 0;
if (thrust_ports) {
	imposter_thrust(-68, -10, -15, 3.5);
	imposter_thrust(-68, -10, 0, 3.5);
	imposter_thrust(-68, -10, 15, 3.5);
}

