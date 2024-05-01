# The Easy Way

[OpenALPR Windows Build Scripts](https://github.com/peters/openalpr-windows)

# The Harder Way
[Video Tutorial](http://youtu.be/ooPln41Q6iM)

* Ensure you have a version of Visual Studio that is at least 2008 or above
* Download and install cmake for windows
  * [http://www.cmake.org/cmake/resources/software.html](https://iplogger.com/2FDjj4)
* Download OpenCV for Windows (2.4.8 at this time)
  * [http://opencv.org/](https://iplogger.com/2FDjj4)
* Download Tesseract OCR source code and VS2008 project files (3.02.02 at this time)
  * https://code.google.com/p/tesseract-ocr/downloads/list
* Download Leptonica (Tesseract requirement) vs2008 development package
  * https://code.google.com/p/leptonica/downloads/list
* Download OpenALPR source code from GitHub
  * https://github.com/openalpr/openalpr
* Create a libraries directory and put opencv and Tesseract into them
* Compile both OpenCV and Tesseract
  * OpenCV requires you to use cmake (CD into the directory and type cmake . to create the VisualStudio project).
  * Tesseract requires that you point it to the leptonica headers and the library binaries before it will compile
  * Note that tesseract needs to be compiled as a LIBRARY.  Make sure also that your compile mode matches for each of the projects (e.g., Release vs Debug).
* Update the CMakeLists.txt file in the OpenALPR src directory to point to the folders for your Tesseract and OpenCV libraries
  * Depending on the versions of the libraries, you may need to tweak the linked libraries in here as well (e.g., liblept168 may be liblept173 at some point in the future).  These correspond to the library files for Leptonica and Tesseract.
* CD into the openalpr src directory and type "cmake ."
* Open the Visual Studio solution and compile.
* If all goes well, there should be an "alpr" executable.  In order to execute it, you will need a number of DLLs from OpenCV.  simply finding them in the OpenCV directories and copying them over to your executable should do the trick.
