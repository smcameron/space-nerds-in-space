
/* draw x, y, and z axis with labels */

draw_labels=1;

/* make mesh radius ~25 */
scale([12,12,12]) {
	union() {
		translate([-0.05,-0.05,-0.05])
			cube([0.1,0.1,1.05]);
		translate([-0.05,-0.05,-0.05])
			cube([1.05,0.1,0.1]);
		translate([-0.05,-0.05,-0.05])
			cube([0.1,1.05,0.1]);
	}

	if (draw_labels) {
		translate([0,0,1.1])
			rotate(180,[0,0,1])
				scale([0.5,0.5,0.5])
					draw_z();
		translate([0,0,1.7])
			rotate(90,[0,0,1])
				rotate(180,[0,0,1])
					scale([0.5,0.5,0.5])
						draw_z();

		translate([1.1,0,0])
			rotate(90,[1,0,0])
				rotate(90,[0,1,0])
					scale([0.5,0.5,0.5])
						draw_x();
		translate([1.7,0,0])
			rotate(90,[0,1,0])
				scale([0.5,0.5,0.5])
					draw_x();

		translate([0,1.1,0])
			rotate(-90,[1,0,0])
				scale([0.5,0.5,0.5])
					draw_y();
		translate([0,1.7,0])
			rotate(90,[0,1,0])
				rotate(-90,[1,0,0])
					scale([0.5,0.5,0.5])
						draw_y();
	}
}

module draw_x() {
	intersection() {
		translate([0,0,0.5])
			cube([0.75,0.1,1],center=true);
		translate([-0.375,0,0])
			union() {
				translate([0.375,0,0.5])
					rotate(-asin(0.75),[0,1,0])
						cube([sqrt(1+0.75*0.75),0.1,0.1], center=true);
				translate([0.375,0,0.5])
					rotate(asin(0.75),[0,1,0])
						cube([sqrt(1+0.75*0.75),0.1,0.1], center=true);
			}
	}
}

module draw_y() {
	intersection() {
		translate([0,0,0.5])
			cube([0.75,0.1,1],center=true);
		translate([-0.375,0,0])
			union() {
				translate([0.375,0,0.26])
					cube([0.1,0.1,0.6],center=true);
				translate([0.75/4.0,0,0.75])
					rotate(asin(0.375/0.5),[0,1,0])
						cube([sqrt(0.5*0.5+0.375*0.375),0.1,0.1], center=true);
				translate([3*0.75/4.0,0,0.75])
					rotate(acos(0.375/0.5)-90,[0,1,0])
						cube([sqrt(0.5*0.5+0.375*0.375),0.1,0.1], center=true);
			}
	}
}

module draw_z() {
	intersection() {
		translate([0,0,0.5])
			cube([0.75,0.1,1],center=true);
		translate([-0.375,-0.05,0])
			union() {
				cube([0.75,0.1,0.1]);
				translate([0,0,0.9])
					cube([0.75,0.1,0.1]);
				rotate(-asin(0.75),[0,1,0])
					cube([sqrt(1+0.75*0.75),0.1,0.1]);
			}
	}
}



