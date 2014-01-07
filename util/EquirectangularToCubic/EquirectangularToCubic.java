/*
Copyright 2009 Zephyr Renner
This file is part of EquirectangulartoCubic.java.
EquirectangulartoCubic is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
EquirectangulartoCubic is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with EquirectangulartoCubic. If not, see http://www.gnu.org/licenses/.

The Equi2Rect.java library is modified from PTViewer 2.8 licenced under the GPL courtesy of Fulvio Senore, originally developed by Helmut Dersch.  Thank you both!

Some basic structural of this code are influenced by / modified from DeepJZoom, Glenn Lawrence, which is also licensed under the GPL.
*/

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.FilenameFilter;
import java.awt.Image;
import java.awt.image.BufferedImage;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import javax.imageio.*;
import javax.imageio.stream.*;
import java.util.Vector;
import java.util.Iterator;


/**
 *
 * @author Zephyr Renner
 */
public class EquirectangularToCubic {

    private enum CmdParseState { DEFAULT, OUTPUTDIR, OVERLAP, INPUTFILE, INTERPOLATION, QUALITY, WIDTH };
    static Boolean deleteExisting = true;
    static String format = "png";

    // The following can be overriden/set by the indicated command line arguments
    static int overlap = 1;           	  // -overlap
    static String interpolation = "lanczos2";   // -interpolation (lanczos2, bilinear, nearest-neighbor)
    static float quality = 0.8f;     // -quality (0.0 to 1.0)
    static int width = 0;		// -width (>0) of output cubemap image
   // static File outputDir = null;         // -outputdir or -o
    static Boolean verboseMode = false;   // -verbose or -v
    static Boolean debugMode = false;     // -debug
    static Vector<File> inputFiles = new Vector<File>();  // must follow all other args
    static Vector<File> outputFiles = new Vector<File>();
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
      
        try {
            try {
				
                parseCommandLine(args);
                // default location for output files is folder beside input files with the name of the input file
				if (outputFiles.size() == 1) {
					File outputFile = outputFiles.get(0);
					if (!outputFile.exists()) {
						File parentFile = outputFile.getAbsoluteFile().getParentFile();
						String fileName = outputFile.getName();
						outputFile = createDir(parentFile, fileName);
					}
				}
				if (outputFiles.size() == 0) {
					Iterator<File> itr = inputFiles.iterator();
			        while (itr.hasNext()) {
			        	File inputFile = itr.next();
                  		File parentFile = inputFile.getAbsoluteFile().getParentFile();
                  		String fileName = inputFile.getName();
        				String nameWithoutExtension = fileName.substring(0, fileName.lastIndexOf('.'));
						File outputFile = createDir(parentFile, "cubic_" + nameWithoutExtension);
						outputFiles.add(outputFile);
                  	}
				}
                if (debugMode) {
                    System.out.printf("overlap=%d ", overlap);
                    System.out.printf("interpolation=%s ", interpolation);
                    System.out.printf("quality=%d ", quality);
                }

            } catch (Exception e) {
                System.out.println("Invalid command: " + e.getMessage());
                return;
            }
            
            Equi2Rect.init();
            
//             Iterator<File> itr = inputFiles.iterator();
//             while (itr.hasNext())
//                  processImageFile(itr.next(), outputDir);
            for( int i = 0; i < inputFiles.size(); i++ ) {
            	processImageFile(inputFiles.get(i), outputFiles.get(i));
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * Process the command line arguments
     * @param args the command line arguments
     */
    private static void parseCommandLine(String[] args) throws Exception {
        CmdParseState state = CmdParseState.DEFAULT;
        for (int count = 0; count < args.length; count++) {
            String arg = args[count];
            switch (state) {
              case DEFAULT:
                  if (arg.equals("-h") || arg.equals("--help")) {
                      System.out.println("");
                      System.out.println("Generate six cubemap images which represent the six orthogonal directions of");
                      System.out.println("a sphere using a Gnomonic Projection with 90 degree field of view.  The input");
                      System.out.println("file is a Cylindrical Equidistant Projection with a 2:1 width to height ratio.");
                      System.out.println("The six cubemap images are in Space Nerds in Space numbering format:");
                      System.out.println("");
                      System.out.println("  +------+");
                      System.out.println("  |  4   |");
                      System.out.println("  |      |");
                      System.out.println("  +------+------+------+------+");
                      System.out.println("  |  0   |  1   |  2   |  3   |");
                      System.out.println("  |      |      |      |      |");
                      System.out.println("  +------+------+------+------+");
                      System.out.println("  |  5   |");
                      System.out.println("  |      |");
                      System.out.println("  +------+");
                      System.out.println("");
                      System.out.println("java -jar EquirectangularToCubic.jar [options] <input_image>");
                      System.out.println("");
                      System.out.println("Options:");
                      System.out.println("  -verbose         extra info output");
                      System.out.println("  -debug           extra debug output");
                      System.out.println("  -outputdir       directory to put generated cubemap images");
                      System.out.println("  -overlap         pixels to overlap cubemap images");
                      System.out.println("  -interpolation   pixel sampling method ()");
                      System.out.println("  -quality         0.0 - 1.0, jpeg compression level");
                      System.out.println("  -width           height and width of generated cubemap images");
                      System.out.println("");
                  } else if (arg.equals("-verbose") || arg.equals("-v"))
                      verboseMode = true;
                  else if (arg.equals("-debug")) {
                      verboseMode = true;
                      debugMode = true;
                  }
                  else if (arg.equals("-outputdir") || arg.equals("-o"))
                      state = CmdParseState.OUTPUTDIR;
                  else if (arg.equals("-overlap"))
                      state = CmdParseState.OVERLAP;
                  else if (arg.equals("-interpolation"))
                      state = CmdParseState.INTERPOLATION;
                  else if (arg.equals("-quality"))
                      state = CmdParseState.QUALITY;
                  else if (arg.equals("-width"))
                      state = CmdParseState.WIDTH;
                  else
                      state = CmdParseState.INPUTFILE;
                  break;
              case OUTPUTDIR:
                  outputFiles.add(new File(args[count]));
                  state = CmdParseState.DEFAULT;
                  break;
              case OVERLAP:
                  overlap = Integer.parseInt(args[count]);
                  state = CmdParseState.DEFAULT;
                  break;
              case INTERPOLATION:
                  interpolation = args[count];
                  state = CmdParseState.DEFAULT;
                  break;
              case QUALITY:
                  quality = Float.parseFloat(args[count]);
                  state = CmdParseState.DEFAULT;
                  break;
              case WIDTH:
                  width = Integer.parseInt(args[count]);
                  state = CmdParseState.DEFAULT;
                  break;
            }
            if (state == CmdParseState.INPUTFILE) {
                File inputFile = new File(arg);
                if (!inputFile.exists())
                    throw new FileNotFoundException("input file not found: " + inputFile.getPath());
                //check if file is folder.
                if (inputFile.isDirectory()){
                	Vector<String> exts = new Vector<String>();
                	exts.add("jpg"); exts.add("jpeg"); exts.add("JPG"); exts.add("JPEG");
                	FilenameFilter select = new FileListFilter(exts);
                	File[] files = inputFile.listFiles(select);
                	java.util.List fileList = java.util.Arrays.asList(files);
					for(java.util.Iterator itr = fileList.iterator(); itr.hasNext();){
						File f = (File) itr.next();
						 inputFiles.add( (File) f);
					}
                }
                else {
                	inputFiles.add(inputFile);
                }
            }
        }
        if (inputFiles.size() == 0)
            throw new Exception("No input files given");
    }

    /**
     * Process the given image file, producing its Deep Zoom output files
     * in a subdirectory of the given output directory.
     * @param inFile the file containing the image
     * @param outputDir the output directory
     */
    private static void processImageFile(File inFile, File outputDir) throws IOException {
        if (verboseMode)
             System.out.printf("Processing image file: %s\n", inFile);

        String fileName = inFile.getName();
        String nameWithoutExtension = fileName.substring(0, fileName.lastIndexOf('.'));
        String pathWithoutExtension = outputDir + File.separator + nameWithoutExtension;

        BufferedImage equi = loadImage(inFile);
        //Image equi = (Image) loadImage(inFile);

        int equiWidth = equi.getWidth();
        int equiHeight = equi.getHeight();
        
        if (equiWidth != equiHeight * 2) {
        	 if (verboseMode)
             System.out.println("Image is not equirectangular (" + equiWidth + " x " + equiHeight + "), skipping...");
             return;
        }
        
        // private static int[][] im_allocate_pano(int ai[][], int i, int j)
		//im_allocate_pano(equiData, equiWidth, equiHeight))
		int equiData[][] = new int[equiHeight][equiWidth];
		new ImageTo2DIntArrayExtractor (equiData, (Image) equi).doit();
		
		double fov; // horizontal field of view
		// peri = 2PIr
		double r = equiWidth / (2D * Math.PI);
		double y = (Math.tan( Math.PI/4D ) * r + overlap);
		fov = Math.atan( y / r ) * 180 / Math.PI * 2;

		int rectWidth;
		int rectHeight;
		if (width > 0) {
			rectWidth = rectHeight = width;
		} else {
			rectWidth = (int) (y * 2);
			rectHeight = rectWidth;
		}

		Boolean bilinear;
		Boolean lanczos2;
		if (interpolation == "lanczos2") { lanczos2 = true; bilinear = false; }
		else if (interpolation == "bilinear") { lanczos2 = false; bilinear = true; }
		else if (interpolation == "nearest-neighbor") { lanczos2 = false; bilinear = false; }
		else { lanczos2 = true; bilinear = false; } // lanczos2 is default for junk values.
		
		int rectData[];
		double pitch;
		double yaw;
		BufferedImage output;
		
		Equi2Rect.initForIntArray2D(equiData);
		
		yaw = 0;
		pitch = 0;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2); 
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		//setRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) 
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-0", quality);
		
		yaw = 90;
		pitch = 0;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2);
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-1", quality);
		
		yaw = 180;
		pitch = 0;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2);
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-2", quality);
		
		yaw = 270;
		pitch = 0;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2);
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-3", quality);
		
		yaw = 0;
		pitch = 90;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2);
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-4", quality);
		
		yaw = 0;
		pitch = -90;
		rectData = Equi2Rect.extractRectilinear(yaw,pitch,fov,equiData,rectWidth,rectHeight,equiWidth,bilinear,lanczos2);
		output = new BufferedImage(rectWidth, rectHeight, BufferedImage.TYPE_INT_RGB);
		output.setRGB(0,0,rectWidth, rectHeight, rectData, 0, rectWidth); 
		saveImageAtQuality(output, outputDir + File.separator + nameWithoutExtension + "-5", quality);
    }

    /**
     * Delete a file
     * @param path the path of the directory to be deleted
     */
    private static void deleteFile(File file) throws IOException {
         if (!file.delete())
             throw new IOException("Failed to delete file: " + file);
    }

    /**
     * Recursively deletes a directory
     * @param path the path of the directory to be deleted
     */
    private static void deleteDir(File dir) throws IOException {
        if (!dir.isDirectory())
            deleteFile(dir);
        else {
            for (File file : dir.listFiles()) {
               if (file.isDirectory())
                   deleteDir(file);
               else
                   deleteFile(file);
            }
            if (!dir.delete())
                throw new IOException("Failed to delete directory: " + dir);
        }
    }

    /**
     * Creates a directory
     * @param parent the parent directory for the new directory
     * @param name the new directory name
     */
    private static File createDir(File parent, String name) throws IOException {
        assert(parent.isDirectory());
        File result = new File(parent + File.separator + name);
        if (result.exists()) return result;
        if (!result.mkdir())
           throw new IOException("Unable to create directory: " + result);
        return result;
    }

    /**
     * Loads image from file
     * @param file the file containing the image
     */
    private static BufferedImage loadImage(File file) throws IOException {
        BufferedImage result = null;
        try {
            result = ImageIO.read(file);
        } catch (Exception e) {
            throw new IOException("Cannot read image file: " + file);
        }
        return result;
    }

    /**
     * Saves image to the given file
     * @param img the image to be saved
     * @param path the path of the file to which it is saved (less the extension)
     */
//     private static void saveImage(BufferedImage img, String path) throws IOException {
//         File outputFile = new File(path + "." + format);
//         try {
//             ImageIO.write(img, format, outputFile);
//         } catch (IOException e) {
//             throw new IOException("Unable to save image file: " + outputFile);
//         }
//     }
    
    /**
     * Saves image to the given file
     * @param img the image to be saved
     * @param path the path of the file to which it is saved (less the extension)
     * @param quality the compression quality to use (0-1)
     */
     
    private static void saveImageAtQuality( BufferedImage img, String path, float quality) throws IOException {
    	File outputFile = new File(path + "." + format);
    	Iterator iter = ImageIO.getImageWritersByFormatName("png");
    	ImageWriter writer = (ImageWriter)iter.next();
    	ImageWriteParam iwp = writer.getDefaultWriteParam();
		FileImageOutputStream output = new FileImageOutputStream(outputFile);
		writer.setOutput(output);
		IIOImage image = new IIOImage(img, null, null);
		try {
			writer.write(null, image, iwp);
		} catch (IOException e) {
            throw new IOException("Unable to save image file: " + outputFile);
        }
		writer.dispose();    	
    }
}


class FileListFilter implements FilenameFilter 
{
	private Vector<String> extensions;
	public FileListFilter(Vector<String> extensions) {
		this.extensions = extensions;
	}
	public boolean accept(File directory, String filename) {
		if (extensions != null) {
			Iterator<String> itr = extensions.iterator();
			while (itr.hasNext()) {
				String ext = (String) itr.next();
				if ( filename.endsWith('.' + ext) ) return true;
			}
		}
		return false;
	}
}
