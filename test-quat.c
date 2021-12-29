#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "mtwist.h"
#include "quat.h"
#include "matrix.h"
#include "mathutils.h"
static int total_tests_failed = 0;
static int total_tests = 0;

#define TESTIT(x, msg) \
	do { \
		total_tests++; \
		if ((x)) { \
			printf("%s:%d: failed.  %s\n", __FILE__, __LINE__, (msg)); \
			total_tests_failed++; \
		} \
	 } while (0)

static void test1()
{
	union quat q, q2, q3;
	union vec3 vi, vo;
	union euler ypr;
	float dx,dy,dz;
	struct mat44 rotm, rhrotm;
	struct mat41 v1, v2, v3;
	int i;

	quat_init_axis(&q, 0, 1, 0, 45 * M_PI / 180.0); /* rotate 45 degrees about y axis */
	vi.v.x = 1;
	vi.v.y = 0;
	vi.v.z = 0;

	quat_rot_vec(&vo, &vi, &q);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	dx = fabs(vo.v.x - sqrt(2.0) / 2.0);
	dy = fabs(vo.v.y - 0.0);
	dz = fabs(vo.v.z - -sqrt(2.0) / 2.0);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");

	quat_init_axis(&q, 0, 0, 1, 45 * M_PI / 180.0); /* rotate 45 degrees about z axis */
	quat_rot_vec(&vo, &vi, &q);
	dx = fabs(vo.v.x - sqrt(2.0) / 2.0);
	dy = fabs(vo.v.y - sqrt(2.0) / 2.0);
	dz = fabs(vo.v.z - 0.0);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");

	vi.v.x = 0;
	vi.v.y = 1;
	vi.v.z = 0;
	quat_init_axis(&q, 1, 0, 0, 45 * M_PI / 180.0); /* rotate 45 degrees about x axis */
	quat_rot_vec(&vo, &vi, &q);
	dx = fabs(vo.v.x - 0.0);
	dy = fabs(vo.v.y - sqrt(2.0) / 2.0);
	dz = fabs(vo.v.z - sqrt(2.0) / 2.0);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");

	/* Vector pointing straight up. */
	vi.v.x = 0;
	vi.v.y = 1;
	vi.v.z = 0;
	quat_init_axis(&q, 1, 0, 0, 90 * M_PI / 180.0); /* rotate 90 degrees about x axis */
	quat_init_axis(&q2, 0, 1, 0, 45 * M_PI / 180.0); /* rotate 45 degrees about y axis */
	quat_mul(&q3, &q2, &q);
	quat_rot_vec(&vo, &vi, &q3);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	dx = fabs(vo.v.x - sqrt(2.0) / 2.0);
	dy = fabs(vo.v.y - 0.0);
	dz = fabs(vo.v.z - sqrt(2.0) / 2.0);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");

	v1.m[0] = 1;
	v1.m[1] = 0;
	v1.m[2] = 0;
	v1.m[3] = 0;
	vi.v.x = v1.m[0];
	vi.v.y = v1.m[1];
	vi.v.z = v1.m[2];
	quat_init_axis(&q, 1, 0, 0, 90 * M_PI / 180.0); /* rotate 90 degrees about x axis */
	quat_init_axis(&q2, 0, 1, 0, 45 * M_PI / 180.0); /* rotate 45 degrees about y axis */
	quat_mul(&q3, &q2, &q);
	quat_to_rh_rot_matrix(&q3, &rotm.m[0][0]);
	mat44_x_mat41(&rotm, &v1, &v2);
	quat_rot_vec(&vo, &vi, &q3);
	printf("x,y,z = %f,%f,%f\n", v2.m[0], v2.m[1], v2.m[2]);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	dx = fabs(v2.m[0] - vo.v.x);
	dy = fabs(v2.m[1] - vo.v.y);
	dz = fabs(v2.m[2] - vo.v.z);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");

	/* Test that quat(0,1,0,45 degrees) rotate (1,0,0) = (.707,0,-.707) */
	v1.m[0] = 1;
	v1.m[1] = 0;
	v1.m[2] = 0;
	v1.m[3] = 0;
	vi.v.x = v1.m[0];
	vi.v.y = v1.m[1];
	vi.v.z = v1.m[2];
	quat_init_axis(&q2, 0, 1, 0, 45 * M_PI / 180.0); /* rotate 45 degrees about y axis */
	quat_to_rh_rot_matrix(&q2, &rotm.m[0][0]);
	mat44_x_mat41(&rotm, &v1, &v2);
	quat_rot_vec(&vo, &vi, &q2);
	printf("x,y,z = %f,%f,%f\n", v2.m[0], v2.m[1], v2.m[2]);
	printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
	dx = fabs(v2.m[0] - vo.v.x);
	dy = fabs(v2.m[1] - vo.v.y);
	dz = fabs(v2.m[2] - vo.v.z);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");
	dx = fabs(v2.m[0] - sqrt(2.0) / 2.0);
	dy = fabs(v2.m[1] - 0);
	dz = fabs(v2.m[2] - -sqrt(2.0) / 2.0);
	TESTIT(dx > 0.00001, "test failed, x wrong\n");
	TESTIT(dz > 0.00001, "test failed, z wrong\n");
	TESTIT(dy > 0.00001, "test failed, y wrong\n");
	quat_to_euler(&ypr, &q2);
	printf("ypr = %f,%f,%f\n", ypr.a.yaw * 180.0 / M_PI,
				ypr.a.pitch * 180.0 / M_PI,
				ypr.a.roll * 180.0 / M_PI);

	for (i = 0; i < 1000; i++) {
		float px, py, pz;

		random_quat(&q);
		random_quat(&q2);
		random_point_on_sphere(1.0, &px, &py, &pz);
		vi.v.x = px;
		vi.v.y = py;
		vi.v.z = pz;
		v1.m[0] = px;
		v1.m[1] = py;
		v1.m[2] = pz;
		v1.m[3] = 0;

		/* compose the 2 random rotations */
		quat_mul(&q3, &q2, &q);
		/* convert to rotation matrix */
		quat_to_rh_rot_matrix(&q3, &rotm.m[0][0]);
		quat_to_lh_rot_matrix(&q3, &rhrotm.m[0][0]);

		/* do rotation with matrix */
		mat44_x_mat41(&rotm, &v1, &v2);

		/* do rotation with matrix */
		mat44_x_mat41(&rhrotm, &v1, &v3);

		/* do rotaiton with quaternion */
		quat_rot_vec(&vo, &vi, &q3);
#if 0
		printf("x,y,z = %f,%f,%f\n", v2.m[0], v2.m[1], v2.m[2]);
		printf("x,y,z = %f,%f,%f\n", vo.v.x, vo.v.y, vo.v.z);
#endif
		dx = fabs(v2.m[0] - vo.v.x);
		dy = fabs(v2.m[1] - vo.v.y);
		dz = fabs(v2.m[2] - vo.v.z);
		TESTIT(dx > 0.001, "test failed, x wrong\n");
		TESTIT(dz > 0.001, "test failed, z wrong\n");
		TESTIT(dy > 0.001, "test failed, y wrong\n");
#if 0
		dx = fabs(v3.m[0] - vo.v.x);
		dy = fabs(v3.m[1] - vo.v.y);
		dz = fabs(v3.m[2] - vo.v.z);
		TESTIT(dx > 0.001, "rh test failed, x wrong\n");
		TESTIT(dz > 0.001, "rh test failed, z wrong\n");
		TESTIT(dy > 0.001, "rh test failed, y wrong\n");
#endif
	}

	for (i = 0; i < 1000; i++) {
		union vec3 v1, v2, v3, v4, up;
		union quat r;
		random_point_on_sphere(1.0, &v1.v.x, &v1.v.y, &v1.v.z);
		random_point_on_sphere(1.0, &v2.v.x, &v2.v.y, &v2.v.z);
		random_point_on_sphere(1.0, &up.v.x, &up.v.y, &up.v.z);
		printf("p1 = %f,%f,%f, p2 = %f,%f,%f\n", v1.v.x, v1.v.y, v1.v.z,
						v2.v.x, v2.v.y, v2.v.z);
		quat_from_u2v(&r, &v1, &v2, &up);
		v3 = v1;
		quat_rot_vec(&v4, &v3, &r);
		dx = fabs(v4.v.x - v2.v.x);
		dy = fabs(v4.v.y - v2.v.y);
		dz = fabs(v4.v.z - v2.v.z);
		TESTIT(dx > 0.001, "quat_from_u2v test failed, x wrong\n");
		TESTIT(dz > 0.001, "quat_from_u2v test failed, z wrong\n");
		TESTIT(dy > 0.001, "quat_from_u2v test failed, y wrong\n");
		printf("got %f, expected %f\n", v4.v.x, v2.v.x);
		printf("got %f, expected %f\n", v4.v.y, v2.v.y);
		printf("got %f, expected %f\n", v4.v.z, v2.v.z);
	}
}

static void test_torus_dist(void)
{
	union vec3 p;
	float dist;

	printf("\n\nTesting torus distance function\n\n");

	p.v.x = 0.0;
	p.v.y = 0.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist) - 15.0 > 1e-5, "point_to_torus_dist failed.\n");
	printf("got %f, expected %f\n", dist, 15.0);

	p.v.x = 20.0;
	p.v.y = 0.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - (sqrtf((20.0 * 20.0) + (20.0 * 20.0)) - 5.0)) > 1e-5, "torus test 1 failed.");
	printf("got %f, expected %f\n", dist, sqrtf((20.0 * 20.0) + (20.0 * 20.0)) - 5.0);

	p.v.x = 0.0;
	p.v.y = 20.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - -5.0) > 1e-5, "torus test 2 failed.");
	printf("got %f, expected %f\n", dist, -5.0);

	p.v.x = 0.0;
	p.v.y = 0.0;
	p.v.z = 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - -5.0) > 1e-5, "torus test 3 failed.");
	printf("got %f, expected %f\n", dist, -5.0);

	p.v.x = 40.0;
	p.v.y = 20.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - (40.0 - 5.0)) > 1e-5, "torus test 4 failed.");
	printf("got %f, expected %f\n", dist, 40.0 - 5.0);

	p.v.x = 0.0;
	p.v.y = 40.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - (20.0 - 5.0)) > 1e-5, "torus test 5 failed.");
	printf("got %f, expected %f\n", dist, 20.0 - 5.0);

	p.v.x = 0.0;
	p.v.y = -40.0;
	p.v.z = 0.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - (20.0 - 5.0)) > 1e-5, "torus test 6 failed.");
	printf("got %f, expected %f\n", dist, 20.0 - 5.0);

	p.v.x = -5.0;
	p.v.y = (1.0 / sqrtf(2.0)) * 20.0;
	p.v.z = (1.0 / sqrtf(2.0)) * 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - 0.0) > 1e-5, "torus test 7 failed.");
	printf("got %f, expected %f\n", dist, 0.0);

	p.v.x = -5.0;
	p.v.y = -(1.0 / sqrtf(2.0)) * 20.0;
	p.v.z = (1.0 / sqrtf(2.0)) * 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - 0.0) > 1e-5, "torus test 7 failed.");
	printf("got %f, expected %f\n", dist, 0.0);

	p.v.x = -5.0;
	p.v.y = -(1.0 / sqrtf(2.0)) * 20.0;
	p.v.z = -(1.0 / sqrtf(2.0)) * 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - 0.0) > 1e-5, "torus test 7 failed.");
	printf("got %f, expected %f\n", dist, 0.0);

	p.v.x = -4.0;
	p.v.y = -(1.0 / sqrtf(2.0)) * 20.0;
	p.v.z = -(1.0 / sqrtf(2.0)) * 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - -1.0) > 1e-5, "torus test 7 failed.");
	printf("got %f, expected %f\n", dist, -1.0);

	p.v.x = -40.0;
	p.v.y = -(1.0 / sqrtf(2.0)) * 20.0;
	p.v.z = -(1.0 / sqrtf(2.0)) * 20.0;
	dist = point_to_torus_dist(&p, 20.0, 5.0);
	TESTIT(fabs(dist - 35.0) > 1e-5, "torus test 7 failed.");
	printf("got %f, expected %f\n", dist, 35.0);
}

static void test_heading_mark_vec3(void)
{
	double h1, m1, h2, m2, h3, m3, r;
	union vec3 v;
	const union vec3 straight_ahead = { { 1.0, 0.0, 0.0 } };
	union quat rotation;

	for (h1 = 0.0; h1 <= 2.0 * M_PI; h1 += M_PI / 180.0) {
		for (m1 = -90.0 * M_PI / 180.0; m1 <= 90.0 * M_PI / 180.0; m1 += M_PI / 180.0) {
			heading_mark_to_vec3(1.0, h1, m1, &v);
			quat_from_u2v(&rotation, &straight_ahead, &v, NULL);
			vec3_to_heading_mark(&v, &r, &h2, &m2);
			quat_to_heading_mark(&rotation, &h3, &m3);
			printf("h1,m1: %f,%f  h2,m2: %f,%f  h3,m3: %f,%f\n",
				h1 * 180.0 / M_PI, m1 * 180.0 / M_PI,
				h2 * 180.0 / M_PI, m2 * 180.0 / M_PI,
				h3 * 180.0 / M_PI, m3 * 180.0 / M_PI);
			TESTIT(fabs(m1 - m2) > 0.5 * M_PI / 180.0, "heading mark vec3 m1m2 test failed.");
			TESTIT(fabs(m1 - m3) > 0.5 * M_PI / 180.0, "heading mark vec3 m1h3 test failed.");
			if (fabs(fabs(m1) - 90.0 * M_PI / 180.0) > 0.01) {
				/* at mark +/- 90, heading can be anything */
				TESTIT(fabs(h1 - h2) > 0.5 * M_PI / 180.0, "heading mark vec3 h1h2 testfailed.");
				TESTIT(fabs(h1 - h3) > 0.5 * M_PI / 180.0, "heading mark vec3 h1h3 test failed.");
			}
		}
	}
}

int main(__attribute__((unused)) int argc, __attribute__((unused))  char *argv[])
{
	test1();
	test_torus_dist();
	test_heading_mark_vec3();
	printf("%d tests failed, %d tests passed.\n", total_tests_failed,
				total_tests - total_tests_failed);
	return 0;
} 
