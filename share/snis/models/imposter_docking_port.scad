
module docking_port(x, y, z, rx, ry, rz, angle, scale) {
	translate([x,y,z]) {
		scale(v = [scale, scale, scale]) {
			rotate(v = [rx, ry, rz], a = angle) {
				rotate(v = [0, 1, 0], a = 90)
					union() {
						cylinder(h = 10, r1 = 30, r2 = 10);
						translate(v = [0, 0, -4])
							cylinder(h = 5, r1 = 30, r2 = 30);
					}
			}
		}
	}
}
