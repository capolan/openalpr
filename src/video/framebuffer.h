#ifndef OPENALPR_FRAMEBUFFER_H
#define OPENALPR_FRAMEBUFFER_H

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>

#include "opencv2/highgui/highgui.hpp"

#include "support/filesystem.h"
#include "support/tinythread.h"
#include "support/platform.h"

int directoryInit (std::string mjpeg_dir);
#endif