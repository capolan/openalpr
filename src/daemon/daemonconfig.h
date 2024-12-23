
#ifndef OPENALPR_DAEMONCONFIG_H
#define	OPENALPR_DAEMONCONFIG_H

#include <string>
#include <vector>
#include "simpleini/simpleini.h"

class DaemonConfig {
public:
  DaemonConfig(std::string config_file, std::string config_defaults_file);
  virtual ~DaemonConfig();

  std::vector<std::string> stream_urls;
  
  std::string country;
  
  int topn;
  int gpio_in;
  int gpio_out;
  int analysis_threads;
  bool alwaysSend;
  bool storePlates;
  bool markImagePlates;
  bool removeImagePlatesFile;
  std::string imageFolder;
  bool uploadData;
  std::string onnx_file;
  std::string upload_url;
  std::string company_id;
  std::string site_id;
  std::string pattern;
  
private:

};

#endif	/* OPENALPR_DAEMONCONFIG_H */

