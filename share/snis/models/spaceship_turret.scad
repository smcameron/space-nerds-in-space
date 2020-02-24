
f_n=16;

module doodad()
{
	cylinder(1.0, 0.5, 0.5, center=true, $fn = f_n);
	cylinder(1.25, 0.3, 0.3, center=true, $fn = 6);
	translate(v = [5.0, 0, 0])
	rotate(v = [0, 1, 0], a = 90) {
		cylinder(10, 0.2, 0.2, center=true, $fn = 8);
	}
}

module gunbarrel()
{
	difference() {
		union() {
			translate([0,0,2]) {
				translate([0,0,-2.5])
					cylinder(0.5,0.25,0.25,center=true,$fn = f_n);
				cylinder(4.5,0.15,0.15,center=true,$fn = f_n);
				translate(v = [0, 0, 2.4])
					cylinder(1.0,0.20,0.15,center=true,$fn = f_n);
				translate(v = [0, 0, 1.85])
					cylinder(0.1, 0.10, 0.20,center=true,$fn = f_n);
				translate(v = [0, 0, 1.80])
					cylinder(0.15, 0.16, 0.16,center=true,$fn = f_n * 4);
			}
			translate(v = [0.31, 0, 2])
				rotate(v = [0, 1, 0], a = 90)
					scale(v = [0.2, 0.2, 0.2])
						doodad();
		}
		translate(v = [0, 0, 4.5])
			cylinder(1.0, 0.10, 0.10, center=true,$fn = f_n);
	}
}

module mount()
{
	union() {
		translate([0,0,0.25])
			cylinder(1,0.3,0.75,$fn = f_n);
		translate([0.9, 0, 0])
			rotate([0,90,0])
				cylinder(3.0, 0.35, 0.22,center=true,$fn = 8);
	}
}

module gun()
{
	union() {
		translate([0,0,-1.5]) {
			rotate([0,90,0])
				rotate(v = [0, 0, 1], 180)
					gunbarrel();
			mount();
		}
		translate([0,0,1.5]) {
			rotate([0,90,0])
				gunbarrel();
			rotate([180,0,0])
				mount();
		}
		cylinder(1.5,0.8,0.8,center=true,$fn = f_n);
	}
}

gun();

