
module cargo_box()
{
	cube(size = [10, 5, 5], center = true);
}

module indentation()
{
	cube(size = [4.3, 4.3, 4.3], center = true);
}

module indentation_pair(angle)
{
	rotate(a = angle, v = [1, 0, 0]) {
		translate(v = [-2.4, 0, 4.4])
				indentation();
		translate(v = [2.4, 0, 4.4])
				indentation();
	}
}

module end_indentations()
{
	translate(v = [-6.9, 0, 0])
		indentation();
	translate(v = [6.9, 0, 0])
		indentation();
}

module cargo_container()
{
	scale(v = [1.5, 1.5, 1.5])
	difference() {
		cargo_box();
		indentation_pair(0);
		indentation_pair(90);
		indentation_pair(180);
		indentation_pair(270);
		end_indentations();
	}
}

cargo_container();

