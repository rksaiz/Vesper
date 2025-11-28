#ifndef READTAGS_H
#define READTAGS_H

#include <iostream>
using std::string;

void ReadAudioTags(const char* filename, string* title, string* artist, string* album, int* year, std::string* date_str = nullptr);
#endif