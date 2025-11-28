#ifndef GETLYRICS_H
#define GETLYRICS_H   

#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>

std::string FetchLyrics(const std::string& title, const std::string& artist);

#endif