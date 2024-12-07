#include "Utils.h"


std::vector<std::string> getTokens(const std::string& str) {
	std::vector<std::string> tokens;
	for (int i = 0; i < str.length(); i++) {
		std::string token = "";
		while (i < str.length() && str[i] != ' ') {
			if (str[i] >= 'A' && str[i] <= 'Z') {
				token += str[i] + 32;
			}
			else {
				token += str[i];
			}
			i++;
		}
		if (tokens.size() == 0)
			tokens.push_back(token);

		else
			tokens.push_back(sanitizePath(token));
	}

	return tokens;
}


bool validateUserCredentials(const std::string& credential) {
	std::string credentialRegex = "^[a-zA-Z0-9_]{1,32}$";
	return std::regex_match(credential, std::regex(credentialRegex));
}


std::string sanitizePath(const std::string& path) {
	std::string blacklistedChars = "~<>\"|?*";
	std::vector<std::string> blacklistedPatterns = { "../" }; // may add more patterns later :)

	std::string sanitizedPath = path;

	sanitizedPath.erase(
		std::remove_if(
			sanitizedPath.begin(),
			sanitizedPath.end(),
			[&](char c) { return blacklistedChars.find(c) != std::string::npos; }
		),
		sanitizedPath.end()
	);

	for (const auto& pattern : blacklistedPatterns) {
		size_t pos;
		while ((pos = sanitizedPath.find(pattern)) != std::string::npos) {
			sanitizedPath.erase(pos, pattern.length());
		}
	}

	return sanitizedPath;
}