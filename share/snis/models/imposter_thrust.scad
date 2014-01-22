module half_shere() {
	difference() {
		sphere(r=1);
		translate([0,-1.5,-1.5])
			cube([3,3,3]);
	}
}
module imposter_thrust(x, y, z, scale) {
	translate([x,y,z]) {
		translate([-0.25,0,0]) {
			scale(scale) {
				union() {
					scale([4,1.4,1.4])
						half_shere();
					scale([1.4,1.4,1.4])
						rotate([0,0,180])
							half_shere();
				}
			}
		}
	}
}
