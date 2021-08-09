#include <libgen.h>
#include "rapidjson/document.h"

#include "config.h"
#include "logging.h"

using namespace Config;

// char* Basename(const char* path) {
//   return nullptr;
// }

config* Config::parseConfig(char const *jsonpath) {
  config* cfg = (config*) malloc(sizeof(config));
  rapidjson::Document document;
  char* buffer = NULL;
  size_t jslen = 0;
  FILE * fp = fopen(jsonpath, "rb");
  if (fp == NULL) {
    Logging::error("Error opening JSON file");
    return nullptr;
  }
  ssize_t bytes_read = getdelim( &buffer, &jslen, '\0', fp);
  if (bytes_read < 0) {
    fclose(fp);
    Logging::error("Error reading JSON file");
    return nullptr;
  }
  fclose(fp);

  char* sock_name = (char*) malloc(strlen(jsonpath) + 3);
  char* jsonpath_ = strdup(jsonpath);
  strcpy(sock_name, basename(jsonpath_));
  free(jsonpath_);
  for (int i = strlen(sock_name); i >= 0; i--) {
    if (sock_name[i] == '.') {
      sock_name[i] = '\x00';
      break;
    }
  }
  strcat(sock_name, ".s");

  if (document.ParseInsitu(buffer).HasParseError()) {
    Logging::error("Error parsing JSON file");
    return nullptr;
  }

  // nfqueue_number
  rapidjson::Value::MemberIterator mi = document.FindMember("nfqueue");
  if (mi == document.MemberEnd()) {
    Logging::error("`nfqueue` not found in configuration");
    return nullptr;
  } else if (!mi->value.IsInt()) {
    Logging::error("`nfqueue` must be a number");
    return nullptr;
  }
  cfg->nfqueue_number = mi->value.GetInt();

  // unix_socket_directory
  mi = document.FindMember("unix_socket_directory");
  if (mi == document.MemberEnd()) {
    Logging::error("`unix_socket_directory` not found in configuration");
    return nullptr;
  } else if (! (mi->value.IsString())) {
    Logging::error("`unix_socket_directory` should be a non-empty string");
    return nullptr;
  }
  char* dir = strdup(mi->value.GetString());
  int dir_len = strlen(dir);
  if (dir_len == 0) {
    Logging::error("`unix_socket_directory` cannot be empty");
    return nullptr;
  }
  if (dir[dir_len-1] != '/') {
    Logging::error("`unix_socket_directory` must end with '/'");
    return nullptr;
  }
  char* path = (char*) malloc(dir_len + strlen(sock_name) + 1);
  memcpy(path, dir, dir_len);
  strcat(path, sock_name);
  cfg->unix_socket = path;

  // free buffer
  free(buffer);

  return cfg;
}