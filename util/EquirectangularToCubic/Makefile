
EquirectangularToCubic.jar : Equi2Rect.java EquirectangularToCubic.java ImageTo2DIntArrayExtractor.java
	javac *.java
	jar cvmf MANIFEST.MF EquirectangularToCubic.jar *.class

clean :
	rm *.class EquirectangularToCubic.jar

help : EquirectangularToCubic.jar
	java -jar EquirectangularToCubic.jar -h

