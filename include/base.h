#ifndef BASE_H
#define BASE_H

#include <string>
#include <random>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>

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

std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;

    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }

    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }

    return fields;
}

std::vector<std::vector<std::string>> ReadCSVColumns(const std::string& filePath, const std::vector<std::string>& columnNames) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        printf("[ReadCSVColumns] Cannot open %s\n", filePath.c_str());
        exit(1);
    }

    std::string line;
     
    // read header
    if (!std::getline(file, line)) {
        printf("[ReadCSVColumns] %s is empty\n", filePath.c_str());
        exit(1);
    }
    const std::vector<std::string> header = splitCSVLine(line);

    std::vector<int> column_index;
    for (std::size_t i = 0; i < columnNames.size(); i++) {
        std::string columnName = columnNames.at(i);
        std::vector<std::string>::const_iterator it = std::find(header.begin(), header.end(), columnName);

        if (it != header.end()) {
            int index = std::distance(header.begin(), it);
            column_index.push_back(index);
        }
        else {
            printf("[ReadCSVColumns] Cannot find column %s in %s\n", columnName.c_str(), filePath.c_str());
            exit(1);
        }
    }

    // read rows
    std::vector<std::vector<std::string>> values;
    for (std::size_t i = 0; i < columnNames.size(); i++) {
        std::vector<std::string> temp_column;
        values.push_back(temp_column);
    }
    while (std::getline(file, line)) {
        const std::vector<std::string> content = splitCSVLine(line);

        for (std::size_t i = 0; i < column_index.size(); i++) {
            int index = column_index.at(i);
            values.at(i).push_back(content.at(index));
        }
    }

    return values;

}

std::vector<std::vector<double>> ReadCSVDoubleColumns(const std::string& filePath, const std::vector<std::string>& columnNames) {
    const std::vector<std::vector<std::string>> stringValues = ReadCSVColumns(filePath, columnNames);

    std::vector<std::vector<double>> values;
    for (std::size_t i = 0; i < stringValues.size(); i++) {
        std::vector<double> temp_column;
        values.push_back(temp_column);
    }

    for (int column_index = 0; column_index < stringValues.size(); column_index++) {
        for (int row_index = 0; row_index < stringValues.at(column_index).size(); row_index++) {
            values.at(column_index).push_back(std::stod(stringValues.at(column_index).at(row_index)));
        }
    }

    return values;
}

#endif 