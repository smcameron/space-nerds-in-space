/*$fn=90; */
$fn=16;

module engine()
{
        rotate(a = 180, v = [0, 1, 0])
        scale(v = [0.15, 0.15, 0.15])
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

module fin()
{
	difference() {
		scale(v = [0.7, 2.5, 0.1])
			sphere(r = 1.7);
		translate(v = [0, 3, 0])
			scale(v = [0.8, 1.6, 0.2])
				sphere(r = 1.4);
	}
}


module main_body()
{
	difference() {
		scale (v = [0.1, 0.8, 1.0])
			sphere(r = 10);
		cube(size = [5, 20, 7], center = true);
		translate(v = [0, 25, 0])
			sphere(r = 20);
	}
}

module fuselage()
{
	scale(v = [0.5, 2, 1])
		translate(v = [0, 2, 0])
			rotate(a = 45, v = [0, 0, 1])
				cube(size = [4, 4, 8], center = true);
	translate(v = [0, 2, 0])
		scale(v = [0.2, 1.0, 0.3])
			difference() {
				sphere(r = 7);
				translate(v = [0, 9.5, 0])
					cube(size = [15, 15, 15], center = true);
			}
	translate(v = [0.5, -2, 0])
		scale(v = [0.4, 1.0, 0.7])
			sphere(r = 2);
}

module fins()
{
	translate(v = [0, 5, 9])
		fin();
	translate(v = [0, 5, -9])
		fin();
}

module an_engine()
{
	rotate(a = 90, v = [1, 0, 0])
		engine();
}

module engines()
{
	union() {
		translate(v = [0, 9.3, 0])
			scale(v = [1.2, 1.2, 1.2])
				an_engine();
		translate(v = [0, 9.3, 2.8])
			scale(v = [0.8, 0.8, 0.8])
				an_engine();
		translate(v = [0, 9.3, -2.8])
			scale(v = [0.8, 0.8, 0.8])
				an_engine();
	}
}

module vanquisher()
{
	rotate(a = 90, v = [0, 0, 1])
		union() {
			main_body();
			fuselage();
			fins();
			engines();
		}
}

vanquisher();

use <imposter_thrust.scad>;
thrust_ports = 0;
if (thrust_ports) {
        imposter_thrust(-12.2, 0, 0, 0.8);
        imposter_thrust(-11.2, 0, 2.8, 0.55);
        imposter_thrust(-11.2, 0, -2.8, 0.55);
}

