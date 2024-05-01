Instructions for compiling on OS X, tested on OS X 10.9.5 (Mavericks).

(Details to follow; see https://github.com/openalpr/openalpr/issues/92 for a narrative.)

## Install with Homebrew (Easy Install)

Using Homebrew:

```sh
brew tap brewsci/science

# openalpr v2.2.0 (https://github.com/Homebrew/homebrew-science/pull/3073)
brew install openalpr

# install HEAD
brew install --HEAD openalpr
```

## Build the package (Instructions for compiling on OS X, tested on OS X 10.12.5. macOS Sierra)

```
brew install tesseract opencv log4cplus

# Clone the latest code from GitHub
git clone https://github.com/openalpr/openalpr.git

# Setup the build directory
cd openalpr/src
mkdir build
cd build

# setup the compile environment
# add flag : CMAKE_MACOSX_RPATH 
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/usr/local -DCMAKE_INSTALL_SYSCONFDIR:PATH=/etc -DCMAKE_MACOSX_RPATH=true -DCMAKE_CXX_FLAGS="-std=c++11"

# compile the library
make

# Install the binaries/libraries to your local system (prefix is /usr/local)
sudo make install

# Copy alpr 
cp alpr alprd /usr/local/bin

# copy runtime_data
mkdir /usr/local/share/openalpr
cp -r ../../runtime_data /usr/local/share/openalpr/

# Test the library
wget http://i.imgur.com/pjukRD0.jpg -O lp.jpg
alpr lp.jpg
```

**NOTICE:**
Please refer to [this issue](https://github.com/openalpr/openalpr/issues/658) if you can't compile.