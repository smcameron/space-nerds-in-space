module cone(height, radius)
{
   /*  difference() { */
        cylinder(h = height, r1 = radius, r2 = 0, center = false);
        /* translate(v = [0, 0, -2])
            cylinder(h = height, r1 = radius, r2 = 0, center = false);
    } */
}

$fn=32;

module wormhole_solid() {
	union() {
	    cone(5,  20);
	    cone(7,  18);
	    cone(10,  15);
	    cone(14,  11);
	    cone(18,  8);
	    cone(23,  5);
	    cone(28,  3);
	    cone(35,  2);
	}
}

module wormhole()
{
	difference() {
		wormhole_solid();
		translate(v = [0, 0, -0.1])
			wormhole_solid();
	}
}

translate(v = [0, 0, -15])
	scale(v = [3, 3, 3])
		wormhole();

