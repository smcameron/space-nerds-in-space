
union() {
	sphere(2);
	translate([0,-0.75,0]) {
		translate([2,0,-1.25])
			rotate([0,90,0])
				cylinder(4.5,0.25,0.25,center=true,$fn=10);
		translate([2,0,1.25])
			rotate([0,90,0])
				cylinder(4.5,0.25,0.25,center=true,$fn=10);
	}
}
