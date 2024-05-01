# Build OpenALPR
1. Install prerequisites
```
sudo apt install -y libopencv-dev libtesseract-dev git cmake build-essential libleptonica-dev liblog4cplus-dev libcurl3-dev
```
1.1. Install beanstalkd to use `alprd` (to use as daemon for video stream etc.)
```
sudo apt install beanstalkd
```
2. Clone the latest code from GitHub
```
git clone https://github.com/openalpr/openalpr.git
```
3. Setup the build directory
```
cd openalpr/src
mkdir build
cd build
```
4. setup the compile environment
```
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_INSTALL_SYSCONFDIR:PATH=/etc ..
```
5. compile the library
```
make
```
6. Install the binaries/libraries to your local system (prefix is /usr)
```
sudo make install
```
You should now have the executables `alpr` in the build folder as well as `openalpr-utils-benchmark`, `openalpr-utils-binarizefontsheet`, `openalpr-utils-sortstate`, `openalpr-utils-calibrate`, `openalpr-utils-tagplates`, `openalpr-utils-classifychars`, `openalpr-utils-benchmark`, and `openalpr-utils-prepcharsfortraining` in build/misc_utilities

## Test
```
# US Plate
wget http://plates.openalpr.com/ea7the.jpg
alpr -c us ea7the.jpg

# EU Plate
wget http://plates.openalpr.com/h786poj.jpg
alpr -c eu h786poj.jpg

# GB Plate
wget https://images.cdn.circlesix.co/image/1/640/0/uploads/posts/2021/07/b22c2b504ab94949ea0df2a8789d0727.jpg
alpr -c gb b22c2b504ab94949ea0df2a8789d0727.jpg
```

# Or compile all dependencies manually

1. Make sure that dependencies and required tools are installed
```
sudo apt-get install -y libpng12-dev libjpeg62-dev libtiff4-dev zlib1g-dev build-essential autoconf automake libtool git-core cmake
```
2. [Install opencv](http://docs.opencv.org/2.4/doc/tutorials/introduction/linux_install/linux_install.html)
3. download and install leptonica and tesseract-ocr (tesseract-ocr requires leptonica and at least one language package)  
```
cd /usr/local/src/openalpr/
wget https://github.com/DanBloomberg/leptonica/archive/v1.70.tar.gz
wget https://github.com/tesseract-ocr/tesseract/archive/refs/tags/3.02.02.tar.gz -O tesseract-ocr-3.02.02.tar.gz
wget https://sourceforge.net/projects/tesseract-ocr-alt/files/tesseract-ocr-3.02.eng.tar.gz
```
4. unpack the tarballs: 
```
tar xf tesseract-ocr-3.02.02.tar.gz 
tar xf tesseract-ocr-3.02.eng.tar.gz
tar xf leptonica-1.70.tar.gz
```
5. compile leptonica:
```
cd  /usr/local/src/openalpr/leptonica-1.70/
./configure --prefix=/usr/local
make
make install
```
6. compile tesseract:
```
cd /usr/local/src/openalpr/tesseract-ocr/
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
```
7. clone the openalpr repo to '/usr/local/src/openalpr/' directory
```
cd /usr/local/src/openalpr/
git clone https://github.com/openalpr/openalpr.git
```
8. update CMakeLists.txt compile openalpr
```
cd /usr/local/src/openalpr/openalpr/
gedit CMakeLists.txt &
    SET(OpenCV_DIR "/usr/local/lib")
    SET(Tesseract_DIR "/usr/local/src/openalpr/tesseract-ocr")
cmake ./
make
```
Note: For Tesseract 3.03 the source files can be downloaded from the main svn branch or https://drive.google.com/folderview?id=0B7l10Bj_LprhQnpSRkpGMGV2eE0&usp=sharing#list. 


## A README to real-time license plate detection with OpenALPR, OpenCV and Python

There are many things which could go possibly wrong if we slightly move in a different direction while installing this package. Follow these instructions for Ubuntu machines to scale this package for real-time detection with OpenCV and Python. (Mac `HomeBrew` is broken too but above the scope of this answer)

1. Start from a clean state, remove every apt-package you've installed for `openaplr` if you've already messed it up. Remove the python binding, if you've set up that too. 
```
apt list --installed | grep alpr
sudo apt-get purge libopenalpr
sudo rm -rf /usr/local/lib/python3.5/dist-packages/openalpr*
```

2.  Follow the official instructions for installing the package on an Ubuntu machine [here](https://github.com/openalpr/openalpr/wiki/Compilation-instructions-(Ubuntu-Linux)). Now, this is the catch. Don't follow `The Easiest Way` described. Follow the `The Easy Way (Ubuntu 14.04+)`. Because, the version which `The Easiest Way` provides is an older one, which doesn't support `recognize_ndarray` functionality for python bindings. 

3. Once you've completed the steps, go to the git repo you've cloned to your machine during the above installation and install the python bindings. `runtime_data` folder is also available in this repo, which will be helpful for next step.

```
cd openalpr/src/bindings/python/
sudo python3 setup.py install
```
4. While declaring the `Alpr` object in the [script](https://github.com/openalpr/openalpr/wiki/Integrating-OpenALPR), what they actually meant by 
```
alpr = Alpr("us", "/path/to/openalpr.conf", "/path/to/runtime_data")
```
is  

```alpr = Alpr("us", "/etc/openalpr/openalpr.conf", "openalpr/runtime_data/")```

5. Now you're all set to grab a video or webcam using `cv2.VideoCapture()`, feed each frame to `alpr.recognize_ndarray()` and life is good 💯 