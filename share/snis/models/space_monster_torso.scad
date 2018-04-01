
$fn=32;

module torso_hull()
{
	union() {
		hull() {
			translate(v = [50, 0, 0])
				scale(v = [1.7, 1, 1])
					sphere(20);
			translate(v = [20, 0, 0])
				scale(v = [1.7, 1, 1])
					sphere(25);
			translate(v = [-50, 0, 0])
				scale(v = [3.7, 1, 1])
					sphere(10);
		}
	}
}

module torso() {
	difference() {
		torso_hull();
		translate(v = [40, 0, 0])
			scale(v = [8, 1.2, 1])
				sphere(12);
		translate(v = [90, 0, 0])
			sphere(25);
	}
	translate(v = [-90, 0, 0])
		scale(v = [0.8, 0.7, 0.5])
			torso_hull();
}

module tooth()
{
	difference() {
	scale(v = [10, 1, 1])
		rotate(v = [1, 0, 0], a = 45)
			rotate(v = [0, 1, 0], a = 45)
				rotate(v = [0, 0, 1], a = 45)
						cube(size = [10, 10, 10], center = true);
	translate(v = [0, -50, -50])
	cube(size = 100, 100, 100);
	}
}

module rotated_tooth(rot)
{
	rotate(v = [0, 0, 1], a = rot)
		translate(v = [150, 0, 0])
			rotate(v = [0, 1, 0], a = 20)
				tooth();
}

module tooth_array() {
	translate(v = [68, 0, 0])
	scale(v = [0.1, 0.1, 0.1])
	rotate(v = [0, 1, 0], a = -90) {
		rotated_tooth(0);
		rotated_tooth(30);
		rotated_tooth(60);
		rotated_tooth(90);
		rotated_tooth(120);
		rotated_tooth(150);
		rotated_tooth(180);
		rotated_tooth(210);
		rotated_tooth(240);
		rotated_tooth(270);
		rotated_tooth(300);
		rotated_tooth(330);
	}
}

module dual_tooth_array() {
	tooth_array();
	translate(v = [-5, 0, 0])
		rotate(v = [1, 0, 0], a = 15)
			tooth_array();
}

/* By splitting the monster in two on the x-z plane, and making
 * the monster in two parts that are separated by a tiny gap, the
 * procedural cylindrical uv-mapping does not cross the international
 * dateline and looks better.
 */
module half_monster() {
	difference() {
		union() {
			torso();
			dual_tooth_array();
		}
		translate(v = [-200, -50, 0])
			cube(size = [300, 100, 100]);
	}
}

module whole_monster() {
	union() {
		translate(v = [0, 0, -0.01])
			half_monster();
		rotate(v = [1, 0, 0], a = 180)
			translate(v = [0, 0, -0.01])
				half_monster();
	}
}

rotate(v = [1, 0, 0], a = 90)
	whole_monster();


