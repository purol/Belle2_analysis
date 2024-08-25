#ifndef BASE_H
#define BASE_H

#include <string>
#include <random>

#include "TSystemDirectory.h"
#include "TList.h"
#include "TSystemFile.h"
#include "TString.h"
#include "TCollection.h"

std::random_device rd;  // Seed for the random number generator
std::mt19937 generator(rd());  // Mersenne Twister random number generator

std::string generateRandomString(size_t length) {
    const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string randomString;
    for (size_t i = 0; i < length; ++i) {
        randomString += characters[distribution(generator)];
    }

    return randomString;
}

bool hasEnding(std::string const& fullString, std::string const& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}


void load_files(const char* dirname, std::vector<std::string>* names) {
    TSystemDirectory dir(dirname, dirname);
    TList* files = dir.GetListOfFiles();
    if (files) {
        TSystemFile* file;
        TString fname;
        TIter next(files);
        while ((file = (TSystemFile*)next())) {
            fname = file->GetName();
            if (!file->IsDirectory() && fname.EndsWith(".root")) {
                names->push_back(fname.Data());
            }
        }
    }
}

void load_files(const char* dirname, std::vector<std::string>* names, const char* included_string) {
    TSystemDirectory dir(dirname, dirname);
    TList* files = dir.GetListOfFiles();
    if (files) {
        TSystemFile* file;
        TString fname;
        TIter next(files);
        while ((file = (TSystemFile*)next())) {
            fname = file->GetName();
            if (!file->IsDirectory() && fname.EndsWith(".root") && fname.Contains(included_string)) {
                names->push_back(fname.Data());
            }
        }
    }
}

#endif 