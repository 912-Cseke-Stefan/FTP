#pragma once

#include <regex>
#include <algorithm>
#include <string>


std::vector<std::string> getTokens(const std::string& str);

bool validateUserCredentials(const std::string& credential);

std::string sanitizePath(const std::string& path);