#include "jobs.h"

#include <iostream>
#include <fstream>
#include <cctype>
#include <mutex>         // <-- add

using namespace std;

static const string FASTA = "human genome.fna";
static const size_t      WIN = 1000000;
static const size_t      OVER = 200000;
static const double      MAX_N = 0.20;
static const size_t      LIMIT = 100;

static ifstream      file;
static string         chrom;
static string         buf;
static size_t              bufPos = 0;
static size_t              absPos = 0;
static bool                finished = false;
static std::mutex jobs_mutex;

vector<Job> q;

static bool fillBuffer() {
    string line;
    while (buf.size() < bufPos + WIN) {
        if (!getline(file, line))
            return false;
        if (line.empty()) continue;
        if (line[0] == '>') {
            chrom.clear();
            chrom = line.substr(1);
            buf.clear();
            bufPos = 0;
            continue;
        }
        for (char c : line)
            if (isalpha(c))
                buf += (char)toupper(c);
    }
    return true;
}

bool nextJob(Job& j) {
    if (finished) return false;
    if (!fillBuffer()) {
        finished = true;
        return false;
    }
    if (buf.size() < bufPos + WIN) {
        finished = true;
        return false;
    }

    size_t nCount = 0;
    for (size_t i = bufPos; i < bufPos + WIN; ++i)
        if (buf[i] == 'N') ++nCount;

    size_t startAbs = absPos;
    string slice = buf.substr(bufPos, WIN);

    bufPos += WIN - OVER;
    absPos += WIN - OVER;

    if (bufPos > OVER) {
        buf.erase(0, bufPos - OVER);
        bufPos = OVER;
    }

    if (double(nCount) / WIN > MAX_N)
        return true;

    j.chrom = chrom;
    j.start = startAbs;
    j.seq = move(slice);
    return true;
}

bool loadJobs() {
    lock_guard<std::mutex> lk(jobs_mutex);
    while (q.size() < LIMIT) {
        Job j;
        if (!nextJob(j)) {
            if (q.empty()) {
                cerr << "No jobs available\n";
                return false;
            }
            cout << "Finished loading jobs\n";
            break;
        };
        q.push_back(move(j));
    }
    return true;
}

int preparejobs() {
    file.open(FASTA);
    if (!file) {
        cerr << "Cannot open " << FASTA << "\n";
        return 1;
    }
    loadJobs();
    cout << "Loaded " << q.size()
        << " jobs; absPos=" << absPos << "\n";
    return 0;
}

string Job::serialize() const
{
    ostringstream oss;
    oss << chrom << '\n'
        << start << '\n'
        << seq << '\n';
    return oss.str();
}

Job Job::deserialize(const string& data)
{
    istringstream iss(data);
    Job job;
    getline(iss, job.chrom);
    string startStr;
    getline(iss, startStr);
    job.start = stoull(startStr);
    getline(iss, job.seq);
    return job;
}

