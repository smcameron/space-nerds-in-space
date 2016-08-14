#include <math.h>
#include <stdint.h>

#include "shield_strength.h"

/* 
 * This function, given a shield profile (strength, width, wavelength) and a probe value
 * returns the shield strength between 0 and 1 at that probe value.  A picture will help.
 * 
 *            shield wavelength
 *                   |
 * 1.0  |            |
 *      | ******     |    ************************ <--- shield strength
 *      |       *    |   * 
 *      |       *    |   *
 *      |        *   |  *
 *      |         *  | *
 *      |         *  | *
 *      |          * |*
 *      |           ** <---------- shield depth
 *      |
 *      |       |<-------->| shield width
 * 0.0  +-----------------------------------------|
 *      10nm     wavelength (nm)                  50nm
 *
 * Every shield profile looks more or less like the above picture, 
 * Having the followign features:
 * 
 * 1. Some baseline level (in the above picture, the baseline is just below 1.0)
 * 2. A dip, or weakness at some point (in the above picture, it's at about 22nm.
 * 3. The dip has some width (about 10nm, above)
 * 4. The dip has some depth (dips down to about 0.3 above.)
 *
 * "sh_strength" 0-255, mapped to 0.0 to 1.0, defines the baseline level.
 *
 * "width", 0-255, mapped to region 10 - 50, and describes the width
 * of the dip.
 *
 * "wavelength" describes the x position 0-255, mapped to 10-50.
 *
 * The "dip" is formed as follows:
 *
 * 0 - width is mapped to 0 - 2*PI, this mapped value is fed to cos(),
 * and this value is multiplied by 0.5 and 0.5 is added (mapping to 0 to 1 range)
 * and this value is then multiplied by sh_strength * 0.9.
 * 
 */

double shield_strength(uint8_t probe, uint8_t sh_strength, uint8_t sh_width,
				uint8_t sh_depth, uint8_t sh_wavelength)
{
	int dip1, dip2;
	double angle, c, depth;

	/* check if probe is outside the dip region.  If so, return baseline level */
	dip1 = sh_wavelength - sh_width / 2.0;
	dip2 = sh_wavelength + sh_width / 2.0;
	if (probe < dip1 || probe > dip2 || dip1 == dip2)
		return (double) sh_strength / 255.0;

	depth = (double) sh_depth / 255.0;
	angle = (double) (probe - dip1) / (double) (dip2 - dip1);
	angle = angle * 2 * M_PI * ((sh_depth & 0x03) + 1);
	c = (cos(angle) * 0.5 + 0.5) * (depth * sh_strength / 255.0) + (1.0 - depth) * ((double) sh_strength / 255.0);

	return c; 
}

