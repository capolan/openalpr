#define DEBUG_CAP

#include <unistd.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <sstream>
#include <iostream>
#include <execinfo.h>
#include <lgpio.h>
#include <fstream>

#include "daemon/beanstalk.hpp"
#include "video/logging_videobuffer.h"
#include "daemon/daemonconfig.h"
#include "inc/safequeue.h"

#include "tclap/CmdLine.h"
#include "alpr.h"
#include "openalpr/cjson.h"
#include "support/tinythread.h"
#include <curl/curl.h>
#include "support/timing.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/fileappender.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/core/types.hpp>

using namespace alpr;

// Variables
SafeQueue<cv::Mat> framesQueue;

// Prototypes
void streamRecognitionThread(void *arg);
bool writeToQueue(std::string jsonResult);
bool uploadPost(CURL *curl, std::string url, std::string data);
void dataUploadThread(void *arg);

// Constants
const std::string ALPRD_CONFIG_FILE_NAME = "alprd.conf";
const std::string OPENALPR_CONFIG_FILE_NAME = "openalpr.conf";
const std::string DEFAULT_LOG_FILE_PATH = "/var/log/alprd.log";

const std::string BEANSTALK_QUEUE_HOST = "127.0.0.1";
const int BEANSTALK_PORT = 11300;
const std::string BEANSTALK_TUBE_NAME = "alprd";

struct CaptureThreadData
{
  std::string company_id;
  std::string stream_url;
  std::string site_id;
  int camera_id;
  int analysis_threads;

  bool clock_on;

  std::string config_file;
  std::string country_code;
  std::string pattern;
  bool output_images;
  bool mark_image_plate;
  bool mark_image_plate_file;
  std::string output_image_folder;
  int top_n;
  int gpio_in;
  int gpio_out;
};

struct UploadThreadData
{
  std::string upload_url;
};

bool daemon_active;

static log4cplus::Logger logger;

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime()
{
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
  // for more information about date/time format
  strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

  return buf;
}
/*---------------INTERRUPT-----------------------------*/
int handleGPIO = -1;
int levelGPIO = -1;
int triggerGPIO = 0;
int pin_out = 0;
bool sigusr1_ok;
bool sigusr2_ok;

#ifdef DEBUG_CAP
uint totalFrame = 0;
#endif

void segfault_handler(int sig)
{
  void *array[10];
  size_t size;

  // CAP
  if (handleGPIO >= 0)
    lgGpiochipClose(handleGPIO);
  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

/* CAP  GPIO interrupt */
void callback_GPIO(int e, lgGpioAlert_p evt, void *data)
{
  int i;
  int userdata = *(int *)data;

  char str[100];

  int level_old = levelGPIO;
  for (i = 0; i < e; i++)
  {
    //      sprintf((char*)str, "u=%d t=%"PRIu64" c=%d g=%d l=%d f=%d (%d of %d)\n",
    //         userdata, evt[i].report.timestamp, evt[i].report.chip,
    //         evt[i].report.gpio, evt[i].report.level,
    //         evt[i].report.flags, i+1, e);
    LOG4CPLUS_INFO(logger, "INT ts:" << evt[i].report.timestamp << " gpio:" << (int)evt[i].report.gpio << " level:" << (int)evt[i].report.level);
    levelGPIO = (int)evt[i].report.level;
  }
  if (level_old != levelGPIO)
  {
    sigusr2_ok = true;
  }
}

int init_GPIO(int pin_in, int pin_out)
{
  static int userdata = 456;
  int i;
  handleGPIO = lgGpiochipOpen(0);
  if (handleGPIO < 0)
  {
    LOG4CPLUS_ERROR(logger, "Failed open GPIO " << lguErrorText(handleGPIO) << " id=" << handleGPIO);
    return -1;
  }
  if (pin_out > 0)
  {
    i = lgGpioClaimOutput(handleGPIO, 0, pin_out, 1);
    if (i < 0)
    {
      LOG4CPLUS_INFO(logger, "GPIO err output:" << i);
    }
    pin_out = 1;
  }
  if (pin_in == 0)
  {
    LOG4CPLUS_INFO(logger, "GPIO trigger in desativado.");
    return 0;
  }
  i = lgGpioClaimInput(handleGPIO, 0, pin_in);
  if (i < 0)
  {
    LOG4CPLUS_INFO(logger, "GPIO err input:" << i);
  }
  levelGPIO = lgGpioRead(handleGPIO, pin_in);
  LOG4CPLUS_INFO(logger, "Init GPIO" << (int)pin_out << "out GPIO" << (int)pin_in << " level:" << (int)levelGPIO);

  lgGpioSetSamplesFunc(callback_GPIO, &userdata);

  lgGpioSetDebounce(handleGPIO, pin_in, 50 * 1000);
  lgGpioClaimAlert(handleGPIO, 0, LG_BOTH_EDGES, pin_in, -1);
  //   lgGpioClaimAlert(handleGPIO, 0, LG_BOTH_EDGES, 24, -1);
  //   lgGpioClaimAlert(handleGPIO, 0, LG_BOTH_EDGES, 17, -1);
  //   lgGpioClaimAlert(handleGPIO, 0, LG_BOTH_EDGES, 27, -1);
  return 0;
}

void sigusr1_handler(int sig)
{
  sigusr1_ok = true;
}

void sigusr2_handler(int sig)
{
  sigusr2_ok = true;
}

struct Tled
{
  int pin;
  int val;
};

void processLed(void *arg)
{
  Tled *dat = (Tled *)arg;
  LOG4CPLUS_INFO(logger, "ThreadLed  " << (int)dat->pin << " level:" << (int)dat->val);
  // tthread::thread* t = new tthread::thread(processingThread, (void*) tdata);
}

/*-----------------------------------------------------*/

int main(int argc, const char **argv)
{
  sigusr1_ok = false;
  signal(SIGSEGV, segfault_handler); // install our segfault handler
  signal(SIGUSR1, sigusr1_handler);  // install our segfault handler
  signal(SIGUSR2, sigusr2_handler);  // install our segfault handler
  daemon_active = true;

  bool noDaemon = false;
  bool clockOn = false;
  std::string logFile;

  std::string configDir;

  TCLAP::CmdLine cmd("OpenAlpr Daemon", ' ', Alpr::getVersion());

  TCLAP::ValueArg<std::string> configDirArg("", "config", "Path to the openalpr config directory that contains alprd.conf and openalpr.conf. (Default: /etc/openalpr/)", false, "/etc/openalpr/", "config_file");
  TCLAP::ValueArg<std::string> logFileArg("l", "log", "Log file to write to.  Default=" + DEFAULT_LOG_FILE_PATH, false, DEFAULT_LOG_FILE_PATH, "topN");

  TCLAP::SwitchArg daemonOffSwitch("f", "foreground", "Set this flag for debugging.  Disables forking the process as a daemon and runs in the foreground.  Default=off", cmd, false);
  TCLAP::SwitchArg clockSwitch("", "clock", "Display timing information to log.  Default=off", cmd, false);

  try
  {

    cmd.add(configDirArg);
    cmd.add(logFileArg);

    if (cmd.parse(argc, argv) == false)
    {
      // Error occurred while parsing.  Exit now.
      return 1;
    }

    // Make sure configDir ends in a slash
    configDir = configDirArg.getValue();
    if (hasEnding(configDir, "/") == false)
      configDir = configDir + "/";

    logFile = logFileArg.getValue();
    noDaemon = daemonOffSwitch.getValue();
    clockOn = clockSwitch.getValue();
  }
  catch (TCLAP::ArgException &e) // catch any exceptions
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    return 1;
  }

  std::string openAlprConfigFile = configDir + OPENALPR_CONFIG_FILE_NAME;
  std::string daemonConfigFile = configDir + ALPRD_CONFIG_FILE_NAME;

  // Validate that the configuration files exist
  if (fileExists(openAlprConfigFile.c_str()) == false)
  {
    std::cerr << "error, openalpr.conf file does not exist at: " << openAlprConfigFile << std::endl;
    return 1;
  }
  if (fileExists(daemonConfigFile.c_str()) == false)
  {
    std::cerr << "error, alprd.conf file does not exist at: " << daemonConfigFile << std::endl;
    return 1;
  }

  if (fileExists("/tmp/videoerr.txt"))
  {
    //     std::filesystem::remove("/tmp/videoerr.txt");
    //     fs::remove("/tmp/videoerr.txt");
    remove("/tmp/videoerr.txt");
  }

  log4cplus::BasicConfigurator config;
  config.configure();

  if (noDaemon == false)
  {
    // Fork off into a separate daemon
    daemon(0, 0);

    log4cplus::SharedAppenderPtr myAppender(new log4cplus::RollingFileAppender(logFile));
    myAppender->setName("alprd_appender");
    // Redirect std out to log file
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("alprd"));
    logger.addAppender(myAppender);

    LOG4CPLUS_INFO(logger, "Running OpenALPR daemon in daemon mode. " << currentDateTime());
  }
  else
  {
    // log4cplus::SharedAppenderPtr myAppender(new log4cplus::ConsoleAppender());
    // myAppender->setName("alprd_appender");
    //  Redirect std out to log file
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("alprd"));
    // logger.addAppender(myAppender);

    LOG4CPLUS_INFO(logger, "Running OpenALPR daemon in the foreground. " << currentDateTime());
  }

  LOG4CPLUS_INFO(logger, "Using: " << daemonConfigFile << " for daemon configuration");

  std::string daemon_defaults_file = INSTALL_PREFIX "/share/openalpr/config/alprd.defaults.conf";
  DaemonConfig daemon_config(daemonConfigFile, daemon_defaults_file);

  if (daemon_config.stream_urls.size() == 0)
  {
    LOG4CPLUS_FATAL(logger, "No video streams defined in the configuration.");
    return 1;
  }

  LOG4CPLUS_INFO(logger, "Using: " << daemon_config.imageFolder << " for storing valid plate images");

  pid_t pid;

  std::vector<tthread::thread *> threads;

  for (int i = 0; i < daemon_config.stream_urls.size(); i++)
  {
    pid = fork();
    if (pid == (pid_t)0)
    {
      // This is the child process, kick off the capture data and upload threads
      CaptureThreadData *tdata = new CaptureThreadData();
      tdata->stream_url = daemon_config.stream_urls[i];
      tdata->camera_id = i + 1;
      tdata->config_file = openAlprConfigFile;
      tdata->mark_image_plate = daemon_config.markImagePlates;
      tdata->mark_image_plate_file = daemon_config.removeImagePlatesFile;
      tdata->output_images = daemon_config.storePlates;
      tdata->output_image_folder = daemon_config.imageFolder;
      tdata->country_code = daemon_config.country;
      tdata->company_id = daemon_config.company_id;
      tdata->site_id = daemon_config.site_id;
      tdata->analysis_threads = daemon_config.analysis_threads;
      tdata->top_n = daemon_config.topn;
      tdata->gpio_in = daemon_config.gpio_in;
      tdata->gpio_out = daemon_config.gpio_out;
      tdata->pattern = daemon_config.pattern;
      tdata->clock_on = clockOn;

      init_GPIO(tdata->gpio_in, tdata->gpio_out);
      tthread::thread *thread_recognize = new tthread::thread(streamRecognitionThread, (void *)tdata);
      threads.push_back(thread_recognize);

      if (daemon_config.uploadData)
      {
        // Kick off the data upload thread
        UploadThreadData *udata = new UploadThreadData();
        udata->upload_url = daemon_config.upload_url;
        tthread::thread *thread_upload = new tthread::thread(dataUploadThread, (void *)udata);

        threads.push_back(thread_upload);
      }

      break;
    }
    // Parent process will continue and spawn more children
  }
  LOG4CPLUS_INFO(logger, ">>>> pid:" << getpid());
  while (daemon_active)
    alpr::sleep_ms(30);
  LOG4CPLUS_INFO(logger, "<<<<");

  for (uint16_t i = 0; i < threads.size(); i++)
    delete threads[i];

  return 0;
}

#ifdef DEBUG_CAP
int64_t s1 = 0, s0 = 0;
#endif

void processingThread(void *arg)
{
  CaptureThreadData *tdata = (CaptureThreadData *)arg;
  Alpr alpr(tdata->country_code, tdata->config_file);
  alpr.setTopN(tdata->top_n);
  alpr.setDefaultRegion(tdata->pattern);

#ifdef DEBUG_CAP
  bool isDir = strstr(tdata->stream_url.c_str(), "dir:");

  if (s0 == 0)
    s0 = s1 = getEpochTimeMs();
  pid_t x = syscall(__NR_gettid);
  LOG4CPLUS_INFO(logger, "Frame start pid:" << getpid() << "  thr:" << x << " isDir:" << isDir);
#endif

  while (daemon_active)
  {

#ifdef DEBUG_CAP
    if ((++totalFrame % 1000) == 0 || sigusr1_ok || sigusr2_ok)
    {
      int64_t s2 = getEpochTimeMs();
      int64_t s = s2 - s1;
      s = s / 1000;
      int64_t ss = s2 - s0;
      ss = ss / 1000;

      // https://stackoverflow.com/questions/6012663/get-unix-timestamp-with-c
      std::time_t t = std::time(nullptr);
      LOG4CPLUS_INFO(logger, "Frame " << s << " sec tot:" << totalFrame << "frame/" << ss << "sec thr:" << x << " " << currentDateTime() << " (" << t << ")");
      s1 = s2;
    }
#endif

    // Wait for a new frame
    cv::Mat frame = framesQueue.pop();
    // Process new frame
    timespec startTime;
    getTimeMonotonic(&startTime);

    std::vector<AlprRegionOfInterest> regionsOfInterest;
    regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));

    AlprResults results = alpr.recognize(frame.data, frame.elemSize(), frame.cols, frame.rows, regionsOfInterest);

    timespec endTime;
    getTimeMonotonic(&endTime);
    double totalProcessingTime = diffclock(startTime, endTime);

    if (tdata->clock_on)
    {
      LOG4CPLUS_INFO(logger, "Camera " << tdata->camera_id << " processed frame in: " << totalProcessingTime << " ms.");
    }
    // CAP
    if (sigusr1_ok || sigusr2_ok || results.plates.size() > 0)
    {

      std::stringstream uuid_ss;
      uuid_ss << tdata->site_id << "-cam" << tdata->camera_id << "-" << getEpochTimeMs();
      std::string uuid = uuid_ss.str();

      // Save the image to disk (using the UUID)
      if (tdata->output_images)
      {
        // CAP grava placa na foto
        if (tdata->mark_image_plate)
        {
          for (unsigned int i = 0; i < results.plates.size(); i++)
          {
            cv::Point pL;
            // Draw a box around the license plate
            for (int z = 0; z < 4; z++)
            {
              AlprCoordinate *coords = results.plates[i].plate_points;
              cv::Point p1(coords[z].x, coords[z].y);
              cv::Point p2(coords[(z + 1) % 4].x, coords[(z + 1) % 4].y);
              cv::line(frame, p1, p2, cv::Scalar(10, 255, 10), 3);
              pL = p2;
            }
            cv::putText(frame, results.plates[i].bestPlate.characters, pL, cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(10, 255, 10), 2, cv::LINE_AA);
          }
        }

        std::stringstream ss;
        ss << tdata->output_image_folder << "/" << uuid << ".jpg";
        cv::imwrite(ss.str(), frame);
        // CAP
        if (sigusr1_ok)
        {
          const std::string tmp = std::string{ss.str()};
          const char *str = tmp.c_str();
          std::stringstream ln;
          ln << tdata->output_image_folder << "/alpr_photo.jpg";
          const std::string tmp2 = std::string{ln.str()};
          const char *str2 = tmp2.c_str();
          unlink(str2);
          symlink(str, str2);
          LOG4CPLUS_INFO(logger, "SIGUSR1 foto");
          lgGpioWrite(handleGPIO, tdata->gpio_out, 1);
          pin_out = 1;
        }
        else if (sigusr2_ok)
        {
          const std::string tmp = std::string{ss.str()};
          const char *str = tmp.c_str();
          std::stringstream ln;
          ln << tdata->output_image_folder << "/alpr_trigger.jpg";
          const std::string tmp2 = std::string{ln.str()};
          const char *str2 = tmp2.c_str();
          unlink(str2);
          symlink(str, str2);
          LOG4CPLUS_INFO(logger, "SIGUSR2 trigger level:" << levelGPIO);
          lgGpioWrite(handleGPIO, tdata->gpio_out, 1);
          pin_out = 1;
        }
        else if (tdata->gpio_out > 0 && pin_out)
        {
          lgGpioWrite(handleGPIO, tdata->gpio_out, 0);
          pin_out = 0;
        }
      }

      // Update the JSON content to include UUID and camera ID
      std::string json = alpr.toJson(results);
      cJSON *root = cJSON_Parse(json.c_str());
      cJSON_AddStringToObject(root, "uuid", uuid.c_str());
      cJSON_AddNumberToObject(root, "camera_id", tdata->camera_id);
      cJSON_AddStringToObject(root, "site_id", tdata->site_id.c_str());
      cJSON_AddNumberToObject(root, "img_width", frame.cols);
      cJSON_AddNumberToObject(root, "img_height", frame.rows);
      // CAP
      if (sigusr1_ok)
      {
        cJSON_ReplaceItemInObject(root, "data_type", cJSON_CreateString("alpr_photo"));
        sigusr1_ok = false;
      }
      if (tdata->gpio_in > 0)
      {
        levelGPIO = lgGpioRead(handleGPIO, tdata->gpio_in);
        cJSON_AddNumberToObject(root, "levelGPIO", levelGPIO);
        cJSON_AddNumberToObject(root, "triggerGPIO", triggerGPIO);
      }
      if (sigusr2_ok)
      {
        cJSON_ReplaceItemInObject(root, "data_type", cJSON_CreateString("alpr_trigger"));
        sigusr2_ok = false;
        triggerGPIO = 0;
      }

      // Add the company ID to the output if configured
      if (tdata->company_id.length() > 0)
        cJSON_AddStringToObject(root, "company_id", tdata->company_id.c_str());

      char *out;
      out = cJSON_PrintUnformatted(root);
      cJSON_Delete(root);

      std::string response(out);

      free(out);

      // Push the results to the Beanstalk queue
      for (int j = 0; j < results.plates.size(); j++)
      {
        LOG4CPLUS_DEBUG(logger, "Writing plate " << results.plates[j].bestPlate.characters << " (" << uuid << ") to queue.");
      }

      writeToQueue(response);
    }
    usleep(10000);
  }
}

void streamRecognitionThread(void *arg)
{
  CaptureThreadData *tdata = (CaptureThreadData *)arg;

  LOG4CPLUS_INFO(logger, "country: " << tdata->country_code << " -- config file: " << tdata->config_file);
  LOG4CPLUS_INFO(logger, "pattern: " << tdata->pattern);
  LOG4CPLUS_INFO(logger, "Stream " << tdata->camera_id << ": " << tdata->stream_url);

  /* Create processing threads */
  const int num_threads = tdata->analysis_threads;
  tthread::thread *threads[num_threads];

  for (int i = 0; i < num_threads; i++)
  {
    LOG4CPLUS_INFO(logger, "Spawning Thread " << i);
    tthread::thread *t = new tthread::thread(processingThread, (void *)tdata);
    threads[i] = t;
  }

  cv::Mat frame;
  LoggingVideoBuffer videoBuffer(logger);
  videoBuffer.connect(tdata->stream_url, tdata->mark_image_plate_file, 5);
  LOG4CPLUS_INFO(logger, "Starting camera " << tdata->camera_id);

  while (daemon_active)
  {
    std::vector<cv::Rect> regionsOfInterest;
    int response = videoBuffer.getLatestFrame(&frame, regionsOfInterest);

    if (response != -1)
    {
      if (framesQueue.empty())
      {
        framesQueue.push(frame.clone());
      }
    }

    usleep(10000);
  }

  videoBuffer.disconnect();
  LOG4CPLUS_INFO(logger, "Video processing ended");
  delete tdata;
  for (int i = 0; i < num_threads; i++)
  {
    delete threads[i];
  }
}

bool writeToQueue(std::string jsonResult)
{
  try
  {
    Beanstalk::Client client(BEANSTALK_QUEUE_HOST, BEANSTALK_PORT);
    client.use(BEANSTALK_TUBE_NAME);

    int id = client.put(jsonResult);

    if (id <= 0)
    {
      LOG4CPLUS_ERROR(logger, "Failed to write data to queue");
      return false;
    }

    LOG4CPLUS_DEBUG(logger, "put job id: " << id);
  }
  catch (const std::runtime_error &error)
  {
    LOG4CPLUS_WARN(logger, "Error connecting to Beanstalk.  Result has not been saved.");
    return false;
  }
  return true;
}

void dataUploadThread(void *arg)
{
  CURL *curl;

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);

  UploadThreadData *udata = (UploadThreadData *)arg;

  while (daemon_active)
  {
    try
    {
      /* get a curl handle */
      curl = curl_easy_init();
      Beanstalk::Client client(BEANSTALK_QUEUE_HOST, BEANSTALK_PORT);

      client.watch(BEANSTALK_TUBE_NAME);

      while (daemon_active)
      {
        Beanstalk::Job job;

        client.reserve(job);

        if (job.id() > 0)
        {
          // LOG4CPLUS_DEBUG(logger, job.body() );
          if (uploadPost(curl, udata->upload_url, job.body()))
          {
            client.del(job.id());
            LOG4CPLUS_INFO(logger, "Job: " << job.id() << " successfully uploaded");
            // Wait 10ms
            sleep_ms(10);
          }
          else
          {
            client.release(job);
            LOG4CPLUS_WARN(logger, "Job: " << job.id() << " failed to upload.  Will retry.");
            // Wait 2 seconds
            sleep_ms(2000);
          }
        }
      }

      /* always cleanup */
      curl_easy_cleanup(curl);
    }
    catch (const std::runtime_error &error)
    {
      LOG4CPLUS_WARN(logger, "Error connecting to Beanstalk.  Will retry.");
    }
    // wait 5 seconds
    usleep(5000000);
  }

  curl_global_cleanup();
}

bool uploadPost(CURL *curl, std::string url, std::string data)
{
  bool success = true;
  CURLcode res;
  struct curl_slist *headers = NULL; // init to NULL is important

  /* Add the required headers */
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charsets: utf-8");

  if (curl)
  {
    /* Add the headers */
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    /* Now specify the POST data */
    // char* escaped_data = curl_easy_escape(curl, data.c_str(), data.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    // curl_free(escaped_data);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK)
    {
      success = false;
    }
  }

  return success;
}
