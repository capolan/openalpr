#ifndef OPENALPR_LOGGING_VIDEOBUFFER_H
#define OPENALPR_LOGGING_VIDEOBUFFER_H
#include "videobuffer.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

class LoggingVideoDispatcher : public VideoDispatcher
{
public:
  LoggingVideoDispatcher(std::string mjpeg_url, bool removeImageFile, int fps, log4cplus::Logger logger) : VideoDispatcher(mjpeg_url, removeImageFile, fps)
  {
    this->logger = logger;
    this->removeImageFile = removeImageFile;
  }

  virtual void log_info(std::string message)
  {
    LOG4CPLUS_INFO(logger, message);
  }
  virtual void log_error(std::string error)
  {
    LOG4CPLUS_WARN(logger, error);
  }

private:
  log4cplus::Logger logger;
  bool removeImageFile;
};

class LoggingVideoBuffer : public VideoBuffer
{
public:
  LoggingVideoBuffer(log4cplus::Logger logger) : VideoBuffer()
  {
    this->logger = logger;
  }

protected:
  virtual VideoDispatcher *createDispatcher(std::string mjpeg_url, bool _removeImageFile, int fps)
  {
    return new LoggingVideoDispatcher(mjpeg_url, _removeImageFile, fps, logger);
  }

private:
  log4cplus::Logger logger;
};

#endif // OPENALPR_LOGGING_VIDEOBUFFER_H