/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
   

public class Equi2Rect {

	static private double mt[][];
    static private long mi[][];
    static final int MI_MULT = 4096;
    static final int MI_SHIFT = 12;
    static final int NATAN = 65536;
    static final int NSQRT = 65536;
	static final int NSQRT_SHIFT = 16;
	
	static int atan_LU_HR[];
	static int sqrt_LU[];
	static double atan_LU[];
	static int PV_atan0_HR;
	static int PV_pi_HR;
	
	////////// Lanczos vars
	// number of subdivisions of the x-axis unity
	//  static int UNIT_XSAMPLES = 1024;
	static int UNIT_XSAMPLES = 256;
	// number of subdivisions of the y-axis unity
	static int UNIT_YSAMPLES = 1024;
	// number of bits to shift to return to the 0-255 range
	// corresponds to the division by (UNIT_YSAMPLES*UNIT_YSAMPLES)
	static int SHIFT_Y = 20;

	// maximum number of weights used to interpolate one pixel
	static int MAX_WEIGHTS = 20;

	// maximum value for the quality parameter
	static int MAX_QUALITY = 6;

	// lookup table
	static int lanczos2_LU[];
	// lookup table for the interpolation weights
	static int lanczos2_weights_LU[][];

	// number of points on each side for an enlarged image
	static int lanczos2_n_points_base = 2;

	// number of points actually used on each side, changes with view_scale
	static int lanczos2_n_points;

	// temporary arrays used during interpolation
	static int aR[], aG[], aB[];

	// current wiewing scale:
	// < 1: pano image is reduced
	// > 1: pano image is enlarged
	static double view_scale;
	/////// end lanczos vars
	
	
	private Equi2Rect() {
	}
	
	static public void init() {
		lanczos2_init();
		math_init();
	}
	static public void initForIntArray2D( int intArray2D[][] ) {
		math_setLookUp(intArray2D);
	}
	
	static public int[] extractRectilinear(
    	double yaw,
    	double pitch,
    	double fov,
    	int equiData[][],
    	int rectWidth,
    	int rectHeight,
    	int equiWidth,
    	Boolean bilinear,
    	Boolean lanczos2 )
    {
    	int returnRectData[] = new int[rectWidth * rectHeight];
		math_extractview(
			equiData, //ai1,
			returnRectData, //vdata,
			rectWidth, //vwidth,
			equiWidth, 
			fov, //hfov,
			yaw, //yaw,
			pitch, //pitch,
			bilinear,
			lanczos2
			);
		return returnRectData;
    }
	
	
	
	
	
	
	
	
	
	private static void math_init() {
		mt = new double[3][3];
		mi = new long[3][3];
	}
	
	private static void math_setLookUp(int ai[][]) {
		if (ai != null) {
			if (atan_LU_HR == null) {
				atan_LU_HR = new int[NATAN + 1];
				atan_LU = new double[NATAN + 1];
				sqrt_LU = new int[NSQRT + 1];
//				double d1 = 0.000244140625D;
				double d1 = 1.0 / (double) NSQRT;
				double d = 0.0D;
				for (int i = 0; i < NSQRT;) {
					sqrt_LU[i] = (int) (Math.sqrt(1.0D + d * d) * NSQRT);
					i++;
					d += d1;
				}

				sqrt_LU[NSQRT] = (int) (Math.sqrt(2D) * NSQRT);
//				d1 = 0.000244140625D;
				d1 = 1.0 / (double) NATAN;
				d = 0.0D;
				for (int j = 0; j < NATAN + 1;) {
					if (j < NATAN)
						atan_LU[j] = Math.atan(d / (1.0D - d)) * 256D;
					else
						atan_LU[j] = 402.12385965949352D;
					j++;
					d += d1;
				}

			}
			math_updateLookUp(ai[0].length);
		}
	}
	
	private static void math_updateLookUp(int i) {
		int j = i << 6;
		if (PV_atan0_HR != j) {
			double dist_e = (double) i / 6.2831853071795862D;
			PV_atan0_HR = j;
			PV_pi_HR = 128 * i;
			for (int k = 0; k < NATAN + 1; k++)
				atan_LU_HR[k] = (int) (dist_e * atan_LU[k] + 0.5D);

		}
	}

	
	private static void math_extractview(
		int pd[][],
		int v[],
		//byte hv[],
		int rectWidth,
		int equiWidth,
		double fov,
		double pan,
		double tilt,
		boolean bilinear,
		boolean lanczos2) {
			
		if(lanczos2) {
			double prev_view_scale = view_scale;
			lanczos2_compute_view_scale(equiWidth, rectWidth, fov);
			if (view_scale != prev_view_scale)
				lanczos2_compute_weights(view_scale);
		}

 		math_set_int_matrix(fov, pan, tilt, rectWidth);
		math_transform(
			pd,
			pd[0].length,
			pd.length, //pd.length + deltaYHorizonPosition,
			v,
			//hv,
			rectWidth,
			v.length / rectWidth,
			fov,
			tilt,
			bilinear,
			lanczos2);
	}
// 
	private static void math_set_int_matrix(double fov, double pan, double tilt, int vw) {
		double a = (fov * 2D * 3.1415926535897931D) / 360D; // field of view in rad
		double p = (double) vw / (2D * Math.tan(a / 2D));
		SetMatrix(
			(tilt * 2D * 3.1415926535897931D) / 360D,
			(pan * 2D * 3.1415926535897931D) / 360D,
			mt,
			1);
		mt[0][0] /= p;
		mt[0][1] /= p;
		mt[0][2] /= p;
		mt[1][0] /= p;
		mt[1][1] /= p;
		mt[1][2] /= p;
		double ta =
			a <= 0.29999999999999999D ? 436906.66666666669D : 131072D / a;
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) 
				mi[j][k] = (long) (ta * mt[j][k] * MI_MULT + 0.5D);
//			mi[j][k] = (int) (ta * mt[j][k] * MI_MULT + 0.5D);
//			mi[j][k] = (int) (ta * mt[j][k] + 0.5D);

		}

	}
// 
// 	/*
// 	 * if bilinear == true use bilinear interpolation
// 	 * if lanczos2 == true use lanczos2 interpolation
// 	 * if bilinear == false && lanczos2 == false use nearest neighbour interpolation
// 	 */
	private static void math_transform(
		int pd[][], //panoData? viz im_loadPano
		int pw, //panoWidth viz extractView: pd[0].length,
		int ph, //panoHeight viz extractView: pd.length + deltaYHorizonPosition,
		int v[], // viewData: viz paint: vdata = new int[vwidth * vheight];
		int vw, //viewWidth
		int vh, //viewHeight viz extractView: v.length / vw,
		double fov,
		double pitch,
		boolean bilinear,
		boolean lanczos2) {

		// flag: use nearest neighbour interpolation
		boolean nn = (!bilinear && !lanczos2);

		boolean firstTime;	// flag
		int itmp;	// temporary variable used as a loop index
		
		int mix = pw - 1;
		int miy = ph - 1; //int miy = ph - deltaYHorizonPosition - 1;
		int w2 = vw - 1 >> 1;
		int h2 = vh >> 1;
		int sw2 = pw >> 1;
		int sh2 = ph >> 1;
		int x_min = -w2;
		int x_max = vw - w2;
		int y_min = -h2;
		int y_max = vh - h2;
		int cy = 0;

		int xs_org, ys_org;	// used for lanczos2 interpolation
		int l24 = 0;
		int pd_0[] = pd[0];
		int pd_1[] = pd[1];
		long m0 = mi[1][0] * y_min + mi[2][0];
		long m1 = mi[1][1] * y_min + mi[2][1];
		long m2 = mi[1][2] * y_min + mi[2][2];
		long mi_00 = mi[0][0];
		long mi_02 = mi[0][2];
		double vfov_2 = fov / 2D; //double vfov_2 = math_fovy(hfov, vw, vh) / 2D;

		// number of points to be computed with linear interpolation
		// between two correctly computed points along the x-axis
		int N_POINTS_INTERP_X = vw/20;
//System.out.println("Max view: " + (pitch + vfov_2) );

//		if (pitch + vfov_2 > 45D || pitch - vfov_2 < -45D) N_POINTS_INTERP_X = vw/30; 
//		if (pitch + vfov_2 > 50D || pitch - vfov_2 < -50D) N_POINTS_INTERP_X = vw/40; 
		if (pitch + vfov_2 > 65D || pitch - vfov_2 < -65D) N_POINTS_INTERP_X = vw/35; 
		if (pitch + vfov_2 > 70D || pitch - vfov_2 < -70D) N_POINTS_INTERP_X = vw/50; 
		if (pitch + vfov_2 > 80D || pitch - vfov_2 < -80D) N_POINTS_INTERP_X = vw/200;
		if (N_POINTS_INTERP_X == 0) N_POINTS_INTERP_X = 1;  // to guarantee value cannot go to 0 and give / 0 errors.
		int N_POINTS_INTERP_X_P1 = N_POINTS_INTERP_X + 1;

		// number of rows to be computed with linear interpolation
		// between two correctly computed rows along the y-axis
		int N_POINTS_INTERP_Y;
		int N_POINTS_INTERP_Y_P1;
		
		///////////////////////////////////////////////////
		// the standard settings cause artifacts at the poles, so we disable interpolation 
		// between rows when we draw the poles
		//
		// since correctly drawing the poles requires very few interpolated points in each row
		// we will interpolate on a larger distance between rows when we are far away from the poles
		// in order to speed up computation
		//
		// so if a pole is in the viewer window things will go this way, considering rows
		// from top to bottom:
		//  - the first rows are very far from the pole and they will be drawn with double
		//    y interpolation to speed up things
		//  - then some rows are nearer to the pole and will be drawn with standard y interpolation
		//  - now we draw the pole without y interpolation
		//  - then we draw some lines with standard y interpolation
		//  - the last lines are farther from the pole and will be drawn with double
		//    y interpolation
		//
		// first row to draw without y-interpolation (default none)
		int N_ROW_NO_INTERP_MIN = y_max + 100;
		// last row to draw without y-interpolation (default none)
		int N_ROW_NO_INTERP_MAX = N_ROW_NO_INTERP_MIN;
		
		// last row of the upper part of the window to draw with double y-interpolation (default none)
		// we will use double distance from row 0 to this row
		int N_ROW_DOUBLE_INTERP_LOW = y_min - 100;
		// first row of the lower part of the window to draw with double y-interpolation (default none)
		// we will use double distance from this row to the last one
		int N_ROW_DOUBLE_INTERP_HIGH = y_max + 100;
		
		if( vfov_2 > 10 ) { // only if not zooming in too much...
			// we consider critical the zone at +/- 5 degrees from the poles
			if (pitch + vfov_2 > 87.5 || pitch - vfov_2 < -87.5) {
				if( pitch > 0 ) {
					// looking upwards
					N_ROW_NO_INTERP_MIN = y_min + (int) ((y_max - y_min)*
							                    (1 - (92.5 - (pitch - vfov_2))/(2*vfov_2)));
					N_ROW_NO_INTERP_MAX = y_min + (int) ((y_max - y_min)*
		                                        (1 - (87.5 - (pitch - vfov_2))/(2*vfov_2)));
				}
				else {
					N_ROW_NO_INTERP_MIN = y_min + (int) ((y_max - y_min)*
		                    					(1 - (-87.5 - (pitch - vfov_2))/(2*vfov_2)));
					N_ROW_NO_INTERP_MAX = y_min + (int) ((y_max - y_min)*
												(1 - (-92.5 - (pitch - vfov_2))/(2*vfov_2)));
					
				}
			}
			// we draw with double y-interpolation the zone outside +/- 10 degrees from the poles
			double angle = 10;
			if (pitch + vfov_2 > 90 - angle || pitch - vfov_2 < -90 + angle) {
				if( pitch > 0 ) {
					// looking upwards
					N_ROW_DOUBLE_INTERP_LOW = y_min + (int) ((y_max - y_min)*
							                    (1 - (90 + angle - (pitch - vfov_2))/(2*vfov_2)));
					N_ROW_DOUBLE_INTERP_HIGH = y_min + (int) ((y_max - y_min)*
		                                        (1 - (90 - angle - (pitch - vfov_2))/(2*vfov_2)));
				}
				else {
					N_ROW_DOUBLE_INTERP_LOW = y_min + (int) ((y_max - y_min)*
		                    					(1 - (-90 + angle - (pitch - vfov_2))/(2*vfov_2)));
					N_ROW_DOUBLE_INTERP_HIGH = y_min + (int) ((y_max - y_min)*
												(1 - (-90 - angle - (pitch - vfov_2))/(2*vfov_2)));
					
				}
			}
//			System.out.println( "Min " + N_ROW_NO_INTERP_MIN + "       Max " + N_ROW_NO_INTERP_MAX );
//			System.out.println( "Low " + N_ROW_DOUBLE_INTERP_LOW + "       High " + N_ROW_DOUBLE_INTERP_HIGH );
		}
		///////////////////////////////////////////////////////////
		
		// data used for interpolation between rows:
		// size of the arrays used to store row values
		int ROWS_INT_SIZE = vw / N_POINTS_INTERP_X + 4;	// just to be safe...
		// coordinates of vertices in the upper computed row
		int[] row_xold = new int[ROWS_INT_SIZE];
		int[] row_yold = new int[ROWS_INT_SIZE];
		// coordinates of vertices in the lower computed row
		int[] row_xnew = new int[ROWS_INT_SIZE];
		int[] row_ynew = new int[ROWS_INT_SIZE];
		// difference between each interpolated line
		int[] row_xdelta = new int[ROWS_INT_SIZE];
		int[] row_ydelta = new int[ROWS_INT_SIZE];
		// used when drawing a line, contains the interpolted values every N_POINTS_INTERP_P1 pixels
		int[] row_xcurrent = new int[ROWS_INT_SIZE];
		int[] row_ycurrent = new int[ROWS_INT_SIZE];
		
		// shifted widh of the panorama
		int pw_shifted = (pw << 8);
		int pw_shifted_2 = pw_shifted / 2;
		int pw_shifted_3 = pw_shifted / 3;

		// used for linear interpolation 
		int x_old;
		int y_old;

		firstTime = true;
		long v0 = m0 + x_min*mi_00;
		long v1 = m1;
		long v2 = m2 + x_min*mi_02;

		N_POINTS_INTERP_Y = N_POINTS_INTERP_X;
		N_POINTS_INTERP_Y_P1 = N_POINTS_INTERP_Y + 1;
		int nPtsInterpXOrg = N_POINTS_INTERP_X;	// stores the original value for future reference
		
		for (int y = y_min; y < y_max;) {
			int idx;
			int x_center, y_center, x_tmp;

			idx = cy;
			
			// if we are drawing one of the poles we disable interpolation between rows
			// to avoid artifacts
			if( (y + N_POINTS_INTERP_Y_P1 > N_ROW_NO_INTERP_MIN) &&
				(y < N_ROW_NO_INTERP_MAX) ) {
				N_POINTS_INTERP_Y = 0;
				if( N_POINTS_INTERP_X != nPtsInterpXOrg ) {
					N_POINTS_INTERP_X = nPtsInterpXOrg;
					firstTime = true;   // to recompute the arrays
				}
			}
			else {
				if( (y + N_POINTS_INTERP_Y_P1 < N_ROW_DOUBLE_INTERP_LOW) ||
					(y > N_ROW_DOUBLE_INTERP_HIGH) ) {
					// we are farther from the pole so we compute more rows with interpolation
					N_POINTS_INTERP_Y = nPtsInterpXOrg * 4;
					// since we are far from the poles we can interpolate between more pixels
					if( N_POINTS_INTERP_X != nPtsInterpXOrg * 4 ) {
						N_POINTS_INTERP_X = nPtsInterpXOrg * 4;
						firstTime = true;   // to recompute the arrays
					}
				} else {
					N_POINTS_INTERP_Y = N_POINTS_INTERP_X;
				}
			}
			N_POINTS_INTERP_Y_P1 = N_POINTS_INTERP_Y + 1;
			N_POINTS_INTERP_X_P1 = N_POINTS_INTERP_X;
//System.out.println( "y = " + y + "  " + N_POINTS_INTERP_Y );			
			
			if( !firstTime ) {
				// row_old[] = row_new[]
				for( itmp = 0; itmp < ROWS_INT_SIZE; itmp++ ) {
					row_xold[itmp] = row_xnew[itmp];
					row_yold[itmp] = row_ynew[itmp];
				}
				m0 += mi[1][0] * N_POINTS_INTERP_Y_P1;
				m1 += mi[1][1] * N_POINTS_INTERP_Y_P1;
				m2 += mi[1][2] * N_POINTS_INTERP_Y_P1;
			}

			// computes row_new[]
			v0 = m0 + x_min*mi_00;
			v1 = m1;
			v2 = m2 + x_min*mi_02;
			int irow = 0;	  // index in the row_*[] arrays
			int curx = x_min;  // x position of the current pixel in the viewer window
			row_xnew[irow] = PV_atan2_HR( (int) v0 >> MI_SHIFT, (int) v2 >> MI_SHIFT);
			row_ynew[irow] = PV_atan2_HR( (int) v1 >> MI_SHIFT, PV_sqrt( (int) Math.abs(v2 >> MI_SHIFT), (int) Math.abs(v0 >> MI_SHIFT)));
//if(firstTime){
//	System.out.println( "row_xnew[0], row_ynew[0]" + row_xnew[irow] + "  " + row_ynew[irow] );
//	System.out.println( "v0, v2 " + (int)(v0 >> MI_SHIFT) + "  " + (int)(v2 >> MI_SHIFT) );
//	System.out.println( PV_pi_HR );
//}
			while( curx <= x_max ) {
				v0 += mi_00 * N_POINTS_INTERP_X_P1;
				v2 += mi_02 * N_POINTS_INTERP_X_P1;
				
				curx += N_POINTS_INTERP_X_P1;
				irow++;
				row_xnew[irow] = PV_atan2_HR( (int) v0 >> MI_SHIFT, (int) v2 >> MI_SHIFT);
				row_ynew[irow] = PV_atan2_HR( (int) v1 >> MI_SHIFT, PV_sqrt( (int) Math.abs(v2 >> MI_SHIFT), (int) Math.abs(v0 >> MI_SHIFT)));
			}
			
			if( firstTime ) {
				// the first time only computes the first row and loops: that computation should be done before the loop
				// but I didn't like the idea of duplicating so much code so I arranged the code in such a way
				firstTime = false;
				continue;
			}

			// computes row_delta[], the difference between each row
			for( itmp = 0; itmp < ROWS_INT_SIZE; itmp++ ) {
				if ((row_xnew[itmp] < -pw_shifted_3) && (row_xold[itmp] > pw_shifted_3))
					row_xdelta[itmp] =
						(row_xnew[itmp] + pw_shifted - row_xold[itmp]) / (N_POINTS_INTERP_Y_P1);
				else {
					if ((row_xnew[itmp] > pw_shifted_3) && (row_xold[itmp] < -pw_shifted_3))
						row_xdelta[itmp] =
							(row_xnew[itmp] - pw_shifted - row_xold[itmp]) / (N_POINTS_INTERP_Y_P1);
					else
						row_xdelta[itmp] = (row_xnew[itmp] - row_xold[itmp]) / (N_POINTS_INTERP_Y_P1);
				}
				row_ydelta[itmp] = (row_ynew[itmp] - row_yold[itmp]) / N_POINTS_INTERP_Y_P1;
			}
			
			// row_current[] contains the values for the current row
			for( itmp = 0; itmp < ROWS_INT_SIZE; itmp++ ) {
				row_xcurrent[itmp] = row_xold[itmp];
				row_ycurrent[itmp] = row_yold[itmp];
			}
			
			// now draws a set of lines
			for( int ky = 0; ky < N_POINTS_INTERP_Y_P1; ky++) {
				
				if( y >= y_max ) break;
				
				irow = 0;
				x_old = row_xcurrent[irow];
				y_old = row_ycurrent[irow];
			
				for (int x = x_min + 1; x <= x_max;) {
					v0 += mi_00 * N_POINTS_INTERP_X_P1;
					v2 += mi_02 * N_POINTS_INTERP_X_P1;
					irow++;
					// determines the next point: it will interpolate between the new and old point
					int x_new = row_xcurrent[irow];
					int y_new = row_ycurrent[irow];
	
					int delta_x;
					if ((x_new < -pw_shifted_3) && (x_old > pw_shifted_3))
						delta_x =
							(x_new + pw_shifted - x_old) / (N_POINTS_INTERP_X_P1);
					else {
						if ((x_new > pw_shifted_3) && (x_old < -pw_shifted_3))
							delta_x =
								(x_new - pw_shifted - x_old) / (N_POINTS_INTERP_X_P1);
						else
							delta_x = (x_new - x_old) / (N_POINTS_INTERP_X_P1);
					}
					int delta_y = (y_new - y_old) / (N_POINTS_INTERP_X_P1);
	
					// now computes the intermediate points with linear interpolation
					int cur_x = x_old;
					int cur_y = y_old;
					for (int kk = 0; kk < N_POINTS_INTERP_X_P1; kk++) {
						if (x > x_max)
							break;
						if (cur_x >= pw_shifted_2)
							cur_x -= pw_shifted;
						if (cur_x < -pw_shifted_2)
							cur_x += pw_shifted;
						cur_y += delta_y;
						int dx = cur_x & 0xff;
						int dy = cur_y & 0xff;
						int xs = (cur_x >> 8) + sw2;
						int ys;
						int v_idx = v[idx];
	
						// used for nn interpolation
						ys_org = (cur_y >> 8) + sh2; //ys_org = (cur_y >> 8) + sh2 - deltaYHorizonPosition;
						int[] pd_row = null;
						int row_index, col_index;
						if( nn ) {
							if( dy < 128 )
								row_index = ys_org;
							else
								row_index = ys_org + 1;
							if( row_index < 0 ) row_index = 0;
							if( row_index > miy ) row_index = miy;
							pd_row = pd[row_index];
						}
						if (v_idx == 0 ) {
							// draws the pixel
							xs_org = xs;
							if (v_idx == 0) {
								if(nn) {
									if( dx < 128 ) 
										col_index = xs_org; 
									else 
										col_index = xs_org + 1;
									if( col_index < 0 ) col_index = 0;
									if( col_index > mix ) col_index = mix;
									int pxl = pd_row[col_index];
									v[idx] = pxl | 0xff000000;
									//hv[idx] = (byte) (pxl >> 24);  //!!!!!!!!!!!!!!!!!!!!!!!!
								}
								else {
									int px00;
									int px01;
									int px10;
									int px11;
									if ((ys = ys_org) == l24
										&& xs >= 0
										&& xs < mix) {
										px00 = pd_0[xs];
										px10 = pd_1[xs++];
										px01 = pd_0[xs];
										px11 = pd_1[xs];
									} else if (
										ys >= 0 && ys < miy && xs >= 0 && xs < mix) {
										l24 = ys;
										pd_0 = pd[ys];
										pd_1 = pd[ys + 1];
										px00 = pd_0[xs];
										px10 = pd_1[xs++];
										px01 = pd_0[xs];
										px11 = pd_1[xs];
									} else {
										if (ys < 0) {
											pd_0 = pd[0];
											l24 = 0;
										} else if (ys > miy) {
											pd_0 = pd[miy];
											l24 = miy;
										} else {
											pd_0 = pd[ys];
											l24 = ys;
										}
										if (++ys < 0)
											pd_1 = pd[0];
										else if (ys > miy)
											pd_1 = pd[miy];
										else
											pd_1 = pd[ys];
										if (xs < 0) {
											px00 = pd_0[mix];
											px10 = pd_1[mix];
										} else if (xs > mix) {
											px00 = pd_0[0];
											px10 = pd_1[0];
										} else {
											px00 = pd_0[xs];
											px10 = pd_1[xs];
										}
										if (++xs < 0) {
											px01 = pd_0[mix];
											px11 = pd_1[mix];
										} else if (xs > mix) {
											px01 = pd_0[0];
											px11 = pd_1[0];
										} else {
											px01 = pd_0[xs];
											px11 = pd_1[xs];
										}
									}
									if(lanczos2)
										//v[idx] = lanczos2_interp_pixel( pd, pw, ph - deltaYHorizonPosition, xs_org, ys_org, dx, dy);
										v[idx] = lanczos2_interp_pixel( pd, pw, ph, xs_org, ys_org, dx, dy);
									else
										v[idx] = bilinear_interp_pixel(px00, px01, px10, px11, dx, dy);
									//hv[idx] = (byte) (px00 >> 24); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
								}
							}
						}
						idx++;
						x++;
						cur_x += delta_x;
					}
					x_old = x_new;
					y_old = y_new;
				}

				// computes the next line using interpolation at the rows level
				for( itmp = 0; itmp < ROWS_INT_SIZE; itmp++ ) {
					row_xcurrent[itmp] += row_xdelta[itmp];
					row_ycurrent[itmp] += row_ydelta[itmp];
				}
				
				y++;
				cy += vw;
			}
		}
	}
// 	
	private static  int bilinear_interp_pixel(int p00, int p01, int p10, int p11, int dx, int dy) {
		int k1 = 255 - dx;
		int l1 = 255 - dy;
		int i2 = k1 * l1;
		int j2 = dy * k1;
		int k2 = dx * dy;
		int l2 = dx * l1;
		int i3 =
			i2 * (p00 >> 16 & 0xff)
				+ l2 * (p01 >> 16 & 0xff)
				+ j2 * (p10 >> 16 & 0xff)
				+ k2 * (p11 >> 16 & 0xff) & 0xff0000;
		int j3 =
			i2 * (p00 >> 8 & 0xff)
				+ l2 * (p01 >> 8 & 0xff)
				+ j2 * (p10 >> 8 & 0xff)
				+ k2 * (p11 >> 8 & 0xff) >> 16;
		int k3 =
			i2 * (p00 & 0xff)
				+ l2 * (p01 & 0xff)
				+ j2 * (p10 & 0xff)
				+ k2 * (p11 & 0xff) >> 16;
		return i3 + (j3 << 8) + k3 + 0xff000000;
	}
// 	
	private static void SetMatrix(double d, double d1, double ad[][], int i) {
		double ad1[][] = new double[3][3];
		double ad2[][] = new double[3][3];
		ad1[0][0] = 1.0D;
		ad1[0][1] = 0.0D;
		ad1[0][2] = 0.0D;
		ad1[1][0] = 0.0D;
		ad1[1][1] = Math.cos(d);
		ad1[1][2] = Math.sin(d);
		ad1[2][0] = 0.0D;
		ad1[2][1] = -ad1[1][2];
		ad1[2][2] = ad1[1][1];
		ad2[0][0] = Math.cos(d1);
		ad2[0][1] = 0.0D;
		ad2[0][2] = -Math.sin(d1);
		ad2[1][0] = 0.0D;
		ad2[1][1] = 1.0D;
		ad2[1][2] = 0.0D;
		ad2[2][0] = -ad2[0][2];
		ad2[2][1] = 0.0D;
		ad2[2][2] = ad2[0][0];
		if (i == 1) {
			matrix_matrix_mult(ad1, ad2, ad);
			return;
		} else {
			matrix_matrix_mult(ad2, ad1, ad);
			return;
		}
	}

	private static void matrix_matrix_mult(
		double ad[][],
		double ad1[][],
		double ad2[][]) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++)
				ad2[i][j] =
					ad[i][0] * ad1[0][j]
						+ ad[i][1] * ad1[1][j]
						+ ad[i][2] * ad1[2][j];

		}
	}
	static int PV_atan2_HR(int pi, int pj)
	{
			long i = pi;
			long j = pj;
			int index;
			if(j > 0)
					if(i > 0)
							return atan_LU_HR[(int)((NATAN * i) / (j + i))];
					else
							return -atan_LU_HR[(int) ((NATAN * -i) / (j - i))];
			if(j == 0)
					if(i > 0)
							return PV_atan0_HR;
					else
							return -PV_atan0_HR;
			if(i < 0) {
				index = (int) ((NATAN * i) / (j + i));
				return atan_LU_HR[index] - PV_pi_HR;
//				return atan_LU_HR[(int) ((NATAN * i) / (j + i))] - PV_pi_HR;
			}
			else
					return -atan_LU_HR[(int) ((NATAN * -i) / (j - i))] + PV_pi_HR;
	}
	
	static int PV_sqrt(int pi, int pj) {
		long i = pi;
		long j = pj;
		if (i > j)
			return (int) (i * sqrt_LU[(int) ((j << NSQRT_SHIFT) / i)] >> NSQRT_SHIFT);
		if (j == 0)
			return 0;
		else
			return (int) (j * sqrt_LU[(int) ((i << NSQRT_SHIFT) / j)] >> NSQRT_SHIFT);
	}
	
// 	
/////////////////////////////////////////////
	// start of Lanczos2 interpolation stuff
	/////////////////////////////////////////////

	private static double sinc(double x) {
		double PI = 3.14159265358979;

		if (x == 0.0)
			return 1.0;
		else
			return Math.sin(PI * x) / (PI * x);
	}	
	
	private static void lanczos2_init() {
		double x, dx;
		int k;

		// sets up the lookup table

		lanczos2_LU = new int[UNIT_XSAMPLES * 2 + 1];
		x = 0.0;
		dx = 1.0 / UNIT_XSAMPLES;
		for (k = 0; k <= UNIT_XSAMPLES * 2; k++) {
			lanczos2_LU[k] =
				(int) (sinc(x) * sinc(x / 2.0) * UNIT_YSAMPLES + 0.5);
			x += dx;
		}

		// allocates the weights lookup table
		// the values are set up by lanczos2_compute_weights()
		lanczos2_weights_LU = new int[UNIT_XSAMPLES + 1][MAX_WEIGHTS];

		// allocates temporary buffers
		aR = new int[MAX_WEIGHTS];
		aG = new int[MAX_WEIGHTS];
		aB = new int[MAX_WEIGHTS];
	}
// 
// 	// computes the weiths for interpolating pixels
// 	// the weights change with view_scale
	private static void lanczos2_compute_weights(double pscale) {
		double s, corr;

		if (pscale > 1.0)
			pscale = 1.0;
		if (pscale >= 1.0)
			lanczos2_n_points = lanczos2_n_points_base;
		else
			lanczos2_n_points = (int) (lanczos2_n_points_base / pscale);

		// sets up the lookup table for the interpolation weights
		for (int j = 0; j <= UNIT_XSAMPLES; j++) {
			// computes the weights for this x value
			int k;
			s = 0;
			int i = j + UNIT_XSAMPLES * (lanczos2_n_points - 1);
			for (k = 0; k < lanczos2_n_points; k++) {
				lanczos2_weights_LU[j][k] =
					lanczos2_LU[(int) (i * pscale + 0.5)];
				s += lanczos2_weights_LU[j][k];
				i -= UNIT_XSAMPLES;
			}
			i = -i;
			for (; k < lanczos2_n_points * 2; k++) {
				lanczos2_weights_LU[j][k] =
					lanczos2_LU[(int) (i * pscale + 0.5)];
				s += lanczos2_weights_LU[j][k];
				i += UNIT_XSAMPLES;
			}
			// normalizes weights so that the sum == UNIT_YSAMPLES
			corr = UNIT_YSAMPLES / s;
			for (k = 0; k < lanczos2_n_points * 2; k++) {
				lanczos2_weights_LU[j][k] =
					(int) (lanczos2_weights_LU[j][k] * corr);
			}
		}
	}

	private static void lanczos2_compute_view_scale(int equiWidth, int rectWidth, double fov) {
		double wDT;

		wDT = fov * equiWidth / 360.0;
		view_scale = rectWidth / wDT;
	}
// 
// 	// interpolates one pixel
	static int lanczos2_interp_pixel(
		int[][] pd,
		int pw,
		int ph,
		int xs,
		int ys,
		int dx,
		int dy) {
		int tmpR, tmpG, tmpB;
		int itl, jtl;
		int ki, kj;
		int i, j;
		int np2, rgb;

		// cordinates of the top-left pixel to be used
		jtl = (xs) - lanczos2_n_points + 1;
		itl = (ys) - lanczos2_n_points + 1;

		// computes the index for the weights lookup table
		int iw = dx;

		// interpolates each row in the x-axis direction
		np2 = lanczos2_n_points * 2;
		//    np2 = lanczos2_n_points << 1;
		i = itl;
		for (ki = 0; ki < np2; ki++) {
			tmpR = tmpG = tmpB = 0;
			j = jtl;
			for (kj = 0; kj < np2; kj++) {
				int r, g, b;
				int i2, j2;

				// checks for out-of-bounds pixels
				i2 = i;
				j2 = j;
				if (i2 < 0)
					i2 = -i2 - 1;
				if (i2 >= ph)
					i2 = ph - (i2 - ph) - 1;
				if (j2 < 0)
					j2 = -j2 - 1;
				if (j2 >= pw)
					j2 = pw - (j2 - pw) - 1;

				rgb = pd[i2][j2];

				r = (rgb >> 16) & 0xff;
				g = (rgb >> 8) & 0xff;
				b = (rgb >> 0) & 0xff;

				int w = lanczos2_weights_LU[iw][kj];

				tmpR += r * w;
				tmpG += g * w;
				tmpB += b * w;
				//		tmpR = tmpR + r*w;
				//		tmpG = tmpG + g*w;
				//		tmpB = tmpB + b*w;

				j++;
			}
			// stores the result for the current row
			aR[ki] = tmpR;
			aG[ki] = tmpG;
			aB[ki] = tmpB;

			i++;
		}

		// computes the index for the weights lookup table
		iw = dy;

		// final interpolation in the y-axis direction
		tmpR = tmpG = tmpB = 0;
		for (ki = 0; ki < np2; ki++) {
			int w = lanczos2_weights_LU[iw][ki];
			tmpR += aR[ki] * w;
			tmpG += aG[ki] * w;
			tmpB += aB[ki] * w;
		}

		tmpR >>= SHIFT_Y;
		tmpG >>= SHIFT_Y;
		tmpB >>= SHIFT_Y;

		if (tmpR > 255)
			tmpR = 255;
		else {
			if (tmpR < 0)
				tmpR = 0;
		}
		if (tmpG > 255)
			tmpG = 255;
		else {
			if (tmpG < 0)
				tmpG = 0;
		}
		if (tmpB > 255)
			tmpB = 255;
		else {
			if (tmpB < 0)
				tmpB = 0;
		}

		return (tmpR << 16) + (tmpG << 8) + tmpB + 0xff000000;
	}


	/////////////////////////////////////////////
	// end of Lanczos2 interpolation stuff
	/////////////////////////////////////////////
}
