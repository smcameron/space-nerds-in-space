
module gunbarrel()
{
	translate([0,0,2]) {
		translate([0,0,-2.5])
			cylinder(0.5,0.25,0.25,center=true,$fn=20);
		cylinder(4.5,0.25,0.13,center=true,$fn=20);
		translate(v = [0, 0, 2.4])
			cylinder(1.0,0.20,0.20,center=true,$fn=20);
	}
}

module mount()
{
	translate([0,0,0.25])
		cylinder(1,0.3,0.75,$fn=20);
	sphere(0.4,center=true,$fn=20);
}

union() {
	translate([0,0,-1.5]) {
		rotate([0,90,0])
			gunbarrel();
		mount();
	}
	translate([0,0,1.5]) {
		rotate([0,90,0])
			gunbarrel();
		rotate([180,0,0])
			mount();
	}

	cylinder(1.5,0.8,0.8,center=true,$fn=20);
}
