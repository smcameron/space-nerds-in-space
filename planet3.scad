
$fn=16;
radius=200;
difference() {
	sphere(r = radius);
	translate(v = [radius * 1.05, 0, 0]) 
		sphere(r = 20);
	translate(v = [radius * 0.85, radius * 0.75, 0.3 * radius]) 
		sphere(r = 50);
	translate(v = [radius * -0.85, radius * -0.75, 0.2 * radius]) 
		sphere(r = 40);
	translate(v = [radius * -0.85, radius * -0.75, 0.2 * radius]) 
		sphere(r = 40);
	translate(v = [radius * -0.25, radius * -0.75, 0.82 * radius]) 
		sphere(r = 47);
	translate(v = [radius * 0.35, radius * -0.65, -0.80 * radius]) 
		sphere(r = 32);
	translate(v = [radius * 0.45, radius * 0.47, -0.88 * radius]) 
		sphere(r = 52);
	translate(v = [radius * -0.75, radius * -0.35, 0.82 * radius]) 
		sphere(r = 27);
	translate(v = [radius * -0.65, radius * -0.35, -0.80 * radius]) 
		sphere(r = 37);
	translate(v = [radius * -0.43, radius * 0.87, -0.38 * radius]) 
		sphere(r = 32);
}

