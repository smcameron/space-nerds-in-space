
module docking_port(x, y, z, rx, ry, rz, angle, scale) {
	translate([x,y,z]) {
		scale(v = [scale, scale, scale]) {
			rotate(v = [rx, ry, rz], a = angle) {
				rotate(v = [0, 1, 0], a = 90)
					union() {
						cylinder(h = 10, r1 = 30, r2 = 10);
						translate(v = [0, 0, -4]) {
							difference() {
								cylinder(h = 5, r1 = 30, r2 = 30);
								cylinder(h = 10, r1 = 36, r2 = 16, center = true);
							}
						}
					}
			}
		}
	}
}

module docking_port2(x, y, z, rx, ry, rz, angle, scale) {
	translate([x,y,z]) {
		scale(v = [scale, scale, scale]) {
			rotate(v = [rx, ry, rz], a = angle) {
				rotate(v = [0, 1, 0], a = 90)
					difference() {
					union() {
						cylinder(h = 10, r1 = 30, r2 = 10);
						translate(v = [0, 0, -4])
							cylinder(h = 5, r1 = 30, r2 = 30);
						cylinder(h = 120, r1 = 29, r2 = 8);
					}
					translate(v = [0, 0, -10])
						cylinder(h = 120, r1 = 27, r2 = 5);
					}
			}
		}
	}
}

module docking_port3(x, y, z, rx, ry, rz, angle, scale) {
	translate([x,y,z]) {
		scale(v = [scale, scale, scale]) {
			rotate(v = [rx, ry, rz], a = angle) {
				cylinder(h = sqrt(2) * 1, r1 = 1, r2 = 0, center = true, $fn = 3);
			}
		}
	}
}

