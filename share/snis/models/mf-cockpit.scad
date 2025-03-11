
small_r = 20;
big_r = 50;
length = 80;
$fn=32;

module cone()
{
	rotate(v = [0, 0, 1], a = -90)
		rotate(v = [1, 0, 0], a = 90)
			cylinder(r1 = small_r, r2 = big_r, h = length);
}

module hollow_cone()
{
	difference() {
		cone();
		translate(v = [-10, 0, 0])
			cone();
	}
}

module central_circle()
{
	translate(v = [20, 0, 0])
		rotate(v = [0, 0, 1], a = -90)
			rotate(v = [1, 0, 0], a = 90)
				cylinder(r1 = small_r * 1.05, r2 = small_r * 1.05, h = length * 2);
}


module hollow_cone_with_circle_window()
{
	difference() {
		hollow_cone();
		central_circle();
	}
}

module strut_cutter()
{
	cube(size = [200, 3, 200], center = true);
}

module struts()
{
	intersection() {
		hollow_cone_with_circle_window();
		strut_cutter();
	}
}

/* hollow_cone_with_circle_window(); */

module hollow_circles()
{
	difference() {
		hollow_cone_with_circle_window();
		translate(v = [ -length * 0.5, 0, 0 ])
			cube(size = [ length * 0.8, big_r * 0.95 * 2.0, big_r * 0.95 * 2.0 ], center = true);
	}
}

rotate(v = [1, 0, 0], a = 45) {
	hollow_circles();
	struts();
	rotate(v = [1, 0, 0], a = 90)
		struts();
}

