#ifndef FASTA_PARSER_H
#define FASTA_PARSER_H
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
struct Job {
    string chrom;
    size_t start;
    string seq;

    string serialize() const;

    static Job deserialize(const string& data);
};
bool nextJob(Job& j);
bool loadJobs();
int preparejobs();
extern vector<Job> q;
#endif 
#pragma once

