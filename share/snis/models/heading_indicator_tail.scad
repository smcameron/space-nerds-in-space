module arrow_feather()
{
	M = [
		[ 1, 0, -1.9, 0   ],
		[ 0, 1,  0.0, 0   ],  // The "0.7" is the skew value; pushed along the y axis
		[ 0, 0,  1.0, 0   ],
		[ 0, 0,  0.0, 1   ]
	];
	translate(v = [0, 0, 0.495])
		multmatrix(M)
			cube(size = [3, 0.05, 1], center = true);
}

module arrow_feather_pair()
{
	union() {
		arrow_feather();
		rotate(v = [1, 0, 0], a = 180)
			arrow_feather();
	}
}

module arrow_feather_set()
{
	difference() {
		union() {
			arrow_feather_pair();
			rotate(v = [1, 0, 0], a = 90)
				arrow_feather_pair();
		}
		translate(v = [3.85, 0, 0])
			cube(size = [1, 1, 1], center = true);
	}
}

arrow_feather_set();

