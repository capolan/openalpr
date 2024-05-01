# The Easy Way


Install prerequisites:

``` bash
sudo dnf install opencv-devel tesseract-devel leptonica-devel git cmake log4cplus-devel libcurl-devel gcc-c++
```

If using the daemon, install beanstalkd
``` bash
sudo dnf install beanstalkd
```

Download, compile and install:

```bash
# Clone the latest code from GitHub
git clone https://github.com/openalpr/openalpr.git

# Setup the build directory
cd openalpr/src

# Setup the compile environment
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_INSTALL_SYSCONFDIR:PATH=/etc ../src

# Compile the library
make

# Install the binaries/libraries to your local system (prefix is /usr)
sudo make install
sudo ldconfig
```

Test the library:

```bash
wget http://plates.openalpr.com/h786poj.jpg -O lp.jpg
alpr lp.jpg
```