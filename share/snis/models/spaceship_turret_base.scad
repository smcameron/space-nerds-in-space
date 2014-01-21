
union() {
	rotate([90,0,0])
		cylinder(0.8,0.75,0.75,$fn=20);
	translate([0,-0.8])
		rotate([90,0,0])
			cylinder(2,1.75,1.75,$fn=20);
}

//include <spaceship_turret.scad>;

