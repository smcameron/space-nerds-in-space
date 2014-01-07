/* ImageTo2DIntArrayExtractor.java
 *
 * Function: rapidly extract pixels from an Image object, storing them in an int array[][].
 * Typical usage: new ImageTo2DIntArrayExtractor(array,xoffset,yoffset,image).doit();
 * Restrictions:
 *   1. assumes the input image has standard ColorModel, if it uses 4-byte pixels.
 *   2. assumes the int array is big enough to hold the image.
 *   3. array index ordering is [y][x].
 *   4. forces pixel alpha = 0xff for compatability with ptviewer.
 * Method: implements ImageConsumer to process pixels from the image's ImageProducer,
 *         placing them directly into the int array.
 * Date: February 22, 2005
 * Author: Rik Littlefield, rj.littlefield at computer.org
 * History: original version Feb 2005 to support Fulvio Senore's version of ptviewer
 * Compatibility: intended and believed compatible with even the earliest JVM's.
 * Coding & documentation style: brief, development code commented in column 1.
 */

import java.util.Hashtable;
import java.awt.image.ImageProducer;
import java.awt.image.ImageConsumer;
import java.awt.image.ColorModel;
import java.awt.Image;

public class ImageTo2DIntArrayExtractor implements ImageConsumer {
	Image myImage;
	int myArray[][];
	ImageProducer myProducer;
	private boolean myWorking = false;
//	int myNumSetPixelCalls = 0; // for debugging & performance testing
	int myXoffset;
	int myYoffset;

	// no default constructor
	private ImageTo2DIntArrayExtractor() {
	}

	// extract image to array starting at [0][0]
	public ImageTo2DIntArrayExtractor (int ai[][], Image image){
		this(ai, 0, 0, image);
	}

	// extract image to array starting at [yoffset][xoffset]
	public ImageTo2DIntArrayExtractor (int ai[][], int xoffset, int yoffset, Image image){
		myArray = ai;
		myImage = image;
		myXoffset = xoffset;
		myYoffset = yoffset;
	}

	// The following method doit is the only one that should be called by the user
	// of ImageTo2DIntArrayExtractor.

	public synchronized void doit() {
//		System.out.println("ImageTo2DIntArrayExtractor::doit called");
//		long t1 = System.currentTimeMillis();
		myProducer = myImage.getSource();
		myWorking = true;
		myProducer.startProduction(this);
		while (myWorking) {
			try {
				wait();
			} catch (InterruptedException e) {
			}
		}
//		long t2 = System.currentTimeMillis();
//		System.out.println("ImageTo2DIntArrayExtractor::Total time = "+(t2-t1));
	}

	// The following methods are required by the ImageConsumer interface.
	// They are called in a separate thread by the image's ImageProducer and
	// should *not* be called directly by the user of ImageTo2DIntArrayExtractor

	public synchronized void setDimensions(int width,int height){ // safely ignore this
//		System.out.println("ImageTo2DIntArrayExtractor::setDimensions called: "+width+","+height);
	}
	public synchronized void setProperties(Hashtable props){ // safely ignore this
	}
	public synchronized void setColorModel(ColorModel model){ // ignore this for now
	}
	public synchronized void setHints(int hintflags){ // safely ignore this
//		System.out.println("ImageTo2DIntArrayExtractor::setHints called: "+hintflags);
	}
	public synchronized void setPixels(int x, int y, int w, int h, ColorModel model,
						  byte[] pixels, int off, int scansize){ // called for gray-scale or indexed
//		System.out.println("ImageTo2DIntArrayExtractor::setPixels byte[] called");
		for (int iy = y; iy < y+h; iy++) {
			int iptr = off + (iy-y)*scansize;
			int row[] = myArray[iy+myYoffset];
			for (int ix = x; ix < x+w; ix++) {
				row[ix+myXoffset] = model.getRGB(pixels[iptr]&0xff) | 0xff000000;
				iptr++;
			}
		}
//		myNumSetPixelCalls++;
	}
	public synchronized void setPixels(int x, int y, int w, int h, ColorModel model,
						  int[] pixels, int off, int scansize){
//		System.out.println("ImageTo2DIntArrayExtractor::setPixels int[] called");
		boolean b = true;
		for (int iy = y; iy < y+h; iy++) {
			if( b ) Thread.yield();	// just to avoid calling yield() every time
			b = !b;
			int iptr = off + (iy-y)*scansize;
			int iyp = iy+myYoffset;
			int row[] = myArray[iy+myYoffset];
			for (int ix = x; ix < x+w; ix++) {
				row[ix+myXoffset] = pixels[iptr] | 0xff000000;
				iptr++;
			}
		}
//		myNumSetPixelCalls++;

//		if (myNumSetPixelCalls % 50 == 0) {  // artificially slow down extraction for testing
//			try {
//				Thread.sleep(50);
//			} catch (InterruptedException e) {
//				// and ignore
//			}
//		}
	}

	public synchronized void imageComplete(int status){
		myProducer.removeConsumer(this);
		myWorking = false;
		notifyAll();
//		System.out.println("ImageTo2DIntArrayExtractor::myNumSetPixelCalls = "+myNumSetPixelCalls);
	}
}
