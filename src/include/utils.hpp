/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <vector>

static std::string replaceAll(std::string str, std::string substr1, std::string substr2) {
    for(size_t index = str.find(substr1, 0); index != std::string::npos && substr1.length(); index = str.find(substr1, index + substr2.length()))
        str.replace(index, substr1.length(), substr2);
    return str;
}

static std::string namespacesToString(std::vector<std::string> namespacesNames, std::string n) {
    std::string ret = n;
    for(int i=0; i<namespacesNames.size(); i++) ret = namespacesNames[i]+"::"+ret;
    return ret;
}

static bool isBasicType(std::string s) {
    return s == "char" || s == "uchar" || s == "short" || s == "ushort" || s == "int" ||s == "uint" || s == "long" || s == "ulong"
    || s == "cent" || s == "ucent" || s == "void" || s == "float" || s == "double";
}

template<typename Base, typename T>
static bool instanceof(const T* ptr) {
    return dynamic_cast<const Base*>(ptr) != nullptr;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define RAVE_OS "WINDOWS"
#elif __linux__
    #define RAVE_OS "LINUX"
#else
    #define RAVE_OS "UNKNOWN"
#endif

#if defined(__x86_64__)
    #define RAVE_PLATFORM "X86_64"
#elif defined(__i386__)
    #define RAVE_PLATFORM "i686"
#elif defined(__arm__)
    #define RAVE_PLATFORM "ARMv7"
#elif defined(__aarch64__)
    #define RAVE_PLATFORM "AARCH64"
#else
    #define RAVE_PLATFORM "UNKNOWN"
#endif

typedef struct genSettings {
    bool noPrelude = false;
    bool runtimeChecks = true;
    bool emitLLVM = false;
    bool onlyObject = false;
    bool noEntry = false;
    bool noStd = false;
    int optLevel = 1;
    bool isPIE = false;
    bool isPIC = false;
    bool disableWarnings = false;
    bool saveObjectFiles = false;
    bool isStatic = false;
    std::string linkParams = "";

    #if defined(__linux__)
        std::string linker = "lld-11";
    #else
        std::string linker = "lld";
    #endif
} genSettings;

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}