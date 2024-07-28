//#define NO_SIGNAL

#include "videobuffer.h"
#include "framebuffer.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

// Inotify INIT

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/signalfd.h>

#include <sys/inotify.h>

/* Structure to keep track of monitored directories */
typedef struct
{
  /* Path of the directory */
  char *path;
  /* inotify watch descriptor */
  int wd;
} monitored_t;

/* Size of buffer to use when reading inotify events */
#define INOTIFY_BUFFER_SIZE 8192

/* Enumerate list of FDs to poll */
enum
{
  FD_POLL_SIGNAL = 0,
  FD_POLL_INOTIFY,
  FD_POLL_MAX
};

/* FANotify-like helpers to iterate events */
#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))
#define IN_EVENT_NEXT(event, length)            \
  ((length) -= (event)->len,                    \
   (struct inotify_event *)(((char *)(event)) + \
                            (event)->len))
#define IN_EVENT_OK(event, length)                  \
  ((long)(length) >= (long)IN_EVENT_DATA_LEN &&     \
   (long)(event)->len >= (long)IN_EVENT_DATA_LEN && \
   (long)(event)->len <= (long)(length))

/* Setup inotify notifications (IN) mask. All these defined in inotify.h. */
static int Xevent_mask =
    (IN_ACCESS |        /* File accessed */
     IN_ATTRIB |        /* File attributes changed */
     IN_OPEN |          /* File was opened */
     IN_CLOSE_WRITE |   /* Writtable File closed */
     IN_CLOSE_NOWRITE | /* Unwrittable File closed */
     IN_CREATE |        /* File created in directory */
     IN_DELETE |        /* File deleted in directory */
     IN_DELETE_SELF |   /* Directory deleted */
     IN_MODIFY |        /* File modified */
     IN_MOVE_SELF |     /* Directory moved */
     IN_MOVED_FROM |    /* File moved away from the directory */
     IN_MOVED_TO);      /* File moved into the directory */

static int event_mask =
    (IN_ATTRIB |      /* File attributes changed */
     IN_CLOSE_WRITE | /* Writtable File closed */
     IN_MODIFY);      /* File modified */

/* Array of directories being monitored */
static monitored_t *monitors;
static int n_monitors;

// END Inotify

using namespace alpr;

const char *get_filename_ext(char *filename)
{
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return NULL;
  return dot + 1;
}

void getALPRFrames(char *imagePath, VideoDispatcher *dispatcher)
{

  // dispatcher->log_info(">getALPRFrames");
  bool hasImage = true;
  try
  {

    cv::Mat frame = cv::imread(imagePath);
    // cv::imshow("Image", frame);
    // cv::waitKey(1000);

    // Double check the image to make sure it's valid.
    if (!frame.data || frame.empty())
    {
      std::stringstream ss;
      ss << "Stream " << dispatcher->mjpeg_url << " received invalid frame";
      dispatcher->log_error(ss.str());
      return;
    }

    dispatcher->mMutex.lock();
    dispatcher->setLatestFrame(frame);
    dispatcher->mMutex.unlock();
  }
  catch (const std::runtime_error &error)
  {
    // Error occured while trying to gather image.  Retry, don't exit.
    std::stringstream ss;
    ss << "Exception happened " << error.what();
    dispatcher->log_error(ss.str());
    dispatcher->mMutex.unlock();
    return;
  }

  dispatcher->mMutex.unlock();
}

static void
__event_process(struct inotify_event *event, VideoDispatcher *dispatcher)
{
  int i;
  static char lastFile[100];
  char *str = (char *)lastFile;
  /* Need to loop all registered monitors to find the one corresponding to the
   * watch descriptor in the event. A hash table here would be quite a better
   * approach. */
  for (i = 0; i < n_monitors; ++i)
  {
    /* If watch descriptors match, we found our directory */
    if (monitors[i].wd == event->wd)
    {
      const char *ext = get_filename_ext(event->name);
      if (ext == NULL)
        continue;
      if (strcmp(ext, "jpg") && strcmp(ext, "jpeg") && strcmp(ext, "png"))
        continue;
      char *pos = strstr(event->name, "part");
      if (pos != NULL)
        continue;
      /*
            if (event->len > 0)
              printf("Received event in '%s/%s': ",
                     monitors[i].path,
                     event->name);
            else
              printf("Received event in '%s': ",
                     monitors[i].path);
      */
      if (event->mask & (IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB))
      {
        char str1[100];
        if (strcmp((char *)lastFile, event->name))
        {
          sprintf((char *)str1, "%s event_mask: %d", event->name, event_mask);
          dispatcher->log_info(str1);
          sleep_ms(20);
          sprintf((char *)str1, "%s/%s", monitors[i].path, event->name);
          getALPRFrames((char *)str1, dispatcher);
          if (dispatcher->removeImageFile == true)
          {
            unlink(str1);
          }
          strcpy(lastFile, event->name);
        }
        else
        {
          sprintf((char *)str1, "%s DUP", event->name);
          dispatcher->log_info(str1);
        }
      }
      return;
    }
  }
}

static void
__shutdown_inotify(int inotify_fd)
{
  int i;

  for (i = 0; i < n_monitors; ++i)
  {
    free(monitors[i].path);
    inotify_rm_watch(inotify_fd, monitors[i].wd);
  }
  free(monitors);
  close(inotify_fd);
}

static int
__initialize_inotify(std::string mjpeg_dir, VideoDispatcher *dispatcher)
{
  int i;
  int inotify_fd;
  char str[100];

  /* Create new inotify device */
  if ((inotify_fd = inotify_init()) < 0)
  {
    sprintf((char *)str, "Couldn't setup new inotify device: '%s'\n", strerror(errno));
    dispatcher->log_error(str);
    return -1;
  }
 
  n_monitors = 0;
  for (int i = 0; mjpeg_dir[i]; i++)
  {
    if (mjpeg_dir[i] == ';')
      n_monitors++;
  }
  n_monitors++;
  /* Allocate array of monitor setups */
  monitors = (monitored_t *)malloc(n_monitors * sizeof(monitored_t));

  /* Loop all input directories, setting up watches */
  // const char *mjpeg_str = mjpeg_dir.c_str();
  char *p = (char *)mjpeg_dir.c_str();
  char *f;
  for (i = 0; i < n_monitors; ++i)
  {
    // Pega diretório da lista de diretório, separador ;
    for (int j = 0; j < i; j++)
    {
      p = (char *)strchr(p, ';');
      p++;
    }
    f = (char *)strchr(p, ';');
    if (f != NULL)
      *f = '\0';
    monitors[i].path = strdup(p);
    if (f != NULL)
      *f = ';';

    if ((monitors[i].wd = inotify_add_watch(inotify_fd,
                                            monitors[i].path,
                                            event_mask)) < 0)
    {
      sprintf((char *)str, "Couldn't add monitor in directory '%s': '%s'", monitors[i].path, strerror(errno));
      dispatcher->log_error(str);
      exit(EXIT_FAILURE);
    }
    sprintf((char *)str, "Started monitoring directory '%s'...", monitors[i].path);
    dispatcher->log_info(str);
  }

  return inotify_fd;
}

static void
__shutdown_signals(int signal_fd)
{
  close(signal_fd);
}

#ifdef NO_SIGNAL
static int
__initialize_signals(VideoDispatcher *dispatcher)
{
  int signal_fd;
  sigset_t sigmask;
  char str[100];

  /* We want to handle SIGINT and SIGTERM in the signal_fd, so we block them. */
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGINT);
  sigaddset(&sigmask, SIGTERM);

  if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0)
  {
    sprintf((char *)str, "Couldn't block signals: %s", strerror(errno));
    dispatcher->log_error(str);
    return -1;
  }

  /* Get new FD to read signals from it */
  if ((signal_fd = signalfd(-1, &sigmask, 0)) < 0)
  {
    sprintf((char *)str, "Couldn't setup signal FD: %s", strerror(errno));
    dispatcher->log_error(str);
    return -1;
  }

  return signal_fd;
}
#endif

int directoryInit(std::string mjpeg_dir, VideoDispatcher *dispatcher)
{
  int signal_fd;
  int inotify_fd;
  struct pollfd fds[FD_POLL_MAX];
  char str[100];
  /* Initialize signals FD */
#ifdef NO_SIGNAL
  if ((signal_fd = __initialize_signals(dispatcher)) < 0)
  {
    dispatcher->log_error("Couldn't initialize signals");
    exit(EXIT_FAILURE);
  }
#endif
  /* Initialize inotify FD and the watch descriptors */
  if ((inotify_fd = __initialize_inotify(mjpeg_dir, dispatcher)) < 0)
  {
    dispatcher->log_info("Couldn't initialize inotify");
    exit(EXIT_FAILURE);
  }

  /* Setup polling */
  fds[FD_POLL_SIGNAL].fd = signal_fd;
  fds[FD_POLL_SIGNAL].events = POLLIN;
  fds[FD_POLL_INOTIFY].fd = inotify_fd;
  fds[FD_POLL_INOTIFY].events = POLLIN;

  /* Now loop */
  while (dispatcher->active)
  {
    while (dispatcher->active)
    {

      /* Block until there is something to be read */
      int rPoll = poll(fds, FD_POLL_MAX, -1);
      if (rPoll < 0)
      {
        sprintf((char *)str, "Couldn't poll(): %s", strerror(errno));
        dispatcher->log_error(str);
        exit(EXIT_FAILURE);
      }
      if (rPoll == 0)
      {
        dispatcher->log_info(".");
        continue;
      }
#ifdef NO_SIGNAL
      /* Signal received? */
      if (fds[FD_POLL_SIGNAL].revents & POLLIN)
      {
        struct signalfd_siginfo fdsi;

        if (read(fds[FD_POLL_SIGNAL].fd,
                 &fdsi,
                 sizeof(fdsi)) != sizeof(fdsi))
        {
          dispatcher->log_info("Couldn't read signal, wrong size read");
          exit(EXIT_FAILURE);
        }

        /* Break loop if we got the expected signal */
        if (fdsi.ssi_signo == SIGINT ||
            fdsi.ssi_signo == SIGTERM)
        {
          dispatcher->log_info("Received SIGINT or SIGTERM");
          break;
        }

        dispatcher->log_info("Received unexpected signal");
      }
#endif
      /* Inotify event received? */
      if (fds[FD_POLL_INOTIFY].revents & POLLIN)
      {
        char buffer[INOTIFY_BUFFER_SIZE];
        size_t length;

        /* Read from the FD. It will read all events available up to
         * the given buffer size. */
        if ((length = read(fds[FD_POLL_INOTIFY].fd,
                           buffer,
                           INOTIFY_BUFFER_SIZE)) > 0)
        {
          struct inotify_event *event;

          event = (struct inotify_event *)buffer;
          while (IN_EVENT_OK(event, length))
          {
            __event_process(event, dispatcher);
            event = IN_EVENT_NEXT(event, length);
          }
        }
      }
      sleep_ms(15);
    }
    sleep_ms(100);
  }
  __shutdown_inotify(inotify_fd);
#ifdef NO_SIGNAL
  if (signal_fd > 0) __shutdown_signals(signal_fd);
#endif
  dispatcher->log_info("Exiting dir lookup");
  return EXIT_SUCCESS;
}
