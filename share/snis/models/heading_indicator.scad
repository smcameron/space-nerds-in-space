translate([2,0,0]) {
	rotate([180,90,0]) {
	   scale([1,1,sqrt(2)])
			translate([0,0,sqrt(2)/2])
			   rotate([0,180,0])
				   cylinder(h = sqrt(2), r1 = 1, r2 = 0, center = true, $fn=4);
		if (0) {
		   translate([0,0,2])
				cylinder(h=2.5,r1=0.5,r2=0.5,cener=true, $fn=4);
		}
	}
}