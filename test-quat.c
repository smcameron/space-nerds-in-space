#include <stdio.h>
#include <math.h>

#include "quat.h"
#include "matrix.h"

#define TESTIT(x, msg) do { if ((x)) printf("%s:%d: failed.  %s\n", __FILE__, __LINE__, (msg)); } while(0);

static void test1()
{
	union quat q, q2, q3;
	union vec3 vi, vo;
	float dx,dy,dz;
	struct mat44 rotm;
	struct mat41 v1, v2;

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
	quat_to_rot_matrix(&q3, &rotm.m[0][0]);
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
}

int main(__attribute__((unused)) int argc, __attribute__((unused))  char *argv[])
{
	test1();
	return 0;
} 
