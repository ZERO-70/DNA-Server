#include <iostream>
#include <fstream>
#include <cctype>
#include <mutex>
#include "TaskManager.h"

using namespace std;

// Define the global variables here (only once)
std::mutex jobs_mutex;
std::ifstream virusfile;
Job virusdata;
std::queue<Task> taskQ;
std::vector<VChunk> vchunks;
std::vector<std::shared_ptr<Socket>> socketRefs;
std::mutex socketRefs_mutex; // Mutex to protect the socketRefs vector

static const string FASTA2 = "DNA2.fna";
static const string FASTA1 = "DNA1.fasta";
static const size_t WIN = 200000;
static const size_t OVER = 40000;
static const double MAX_N = 0.20;
static const size_t LIMIT = 100;

static ifstream file;
static string chrom;
static string buf;
static size_t bufPos = 0;
static size_t absPos = 0;
static bool finished = false;

using std::vector;

static bool fillBuffer()
{
    string line;
    while (buf.size() < bufPos + WIN)
    {
        if (!getline(file, line))
            return false;
        if (line.empty())
            continue;
        if (line[0] == '>')
        {
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

bool nextJob(Job &j)
{
    if (finished)
        return false;
    if (!fillBuffer())
    {
        finished = true;
        return false;
    }
    if (buf.size() < bufPos + WIN)
    {
        finished = true;
        return false;
    }

    size_t nCount = 0;
    for (size_t i = bufPos; i < bufPos + WIN; ++i)
        if (buf[i] == 'N')
            ++nCount;

    size_t startAbs = absPos;
    string slice = buf.substr(bufPos, WIN);

    bufPos += WIN - OVER;
    absPos += WIN - OVER;

    if (bufPos > OVER)
    {
        buf.erase(0, bufPos - OVER);
        bufPos = OVER;
    }

    if (double(nCount) / WIN > MAX_N)
        return nextJob(j); // skip this window and recurse until you find a good one

    j.chrom = chrom;
    j.start = startAbs;
    j.seq = move(slice);
    return true;
}

bool loadTasks()
{
    std::lock_guard<std::mutex> lk(jobs_mutex);

    static Job currentH;
    static size_t nextVidx = 0;
    static bool haveH = false;

    while (taskQ.size() < LIMIT)
    {
        if (!haveH)
        {
            if (!nextJob(currentH))
            {
                if (taskQ.empty())
                {
                    std::cerr << "No jobs available\n";
                    return false;
                }
                break;
            }
            nextVidx = 0;
            haveH = true;
        }

        while (nextVidx < vchunks.size() && taskQ.size() < LIMIT)
        {
            taskQ.emplace(Task{currentH, nextVidx});
            ++nextVidx;
        }

        if (nextVidx >= vchunks.size())
        {
            haveH = false;
        }
    }

    return true;
}

int preparejobs()
{
    file.open(FASTA2);
    if (!file)
    {
        std::cerr << "Cannot open " << FASTA2 << '\n';
        return 1;
    }
    loadTasks();
    std::cout << "Primed " << taskQ.size() << " tasks\n";
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

Job Job::deserialize(const string &data)
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

std::string Task::serialize() const
{
    std::ostringstream oss;
    oss << human.serialize();
    oss << vIndex << '\n';

    // Add bounds checking
    if (vIndex < vchunks.size())
    {
        oss << vchunks[vIndex].vStart << '\n';
        oss << vchunks[vIndex].seq << '\n';
    }
    else
    {
        // Handle error: Invalid vIndex
        std::cerr << "Error: Invalid virus chunk index: " << vIndex
                  << ", max is " << (vchunks.size() - 1) << std::endl;
        oss << "0\n\n"; // Safe default values
    }

    return oss.str();
}

Task Task::deserialize(const std::string &s)
{
    std::istringstream iss(s);
    Task t;
    getline(iss, t.human.chrom);
    std::string tmp;
    getline(iss, tmp);
    t.human.start = std::stoull(tmp);
    getline(iss, t.human.seq);
    getline(iss, tmp);
    t.vIndex = std::stoul(tmp);

    // Read the virus chunk data (but not storing it since client doesn't need to send it back)
    // The client can extract this data for processing
    std::string vStart, vSeq;
    getline(iss, vStart); // Read virus chunk start position
    getline(iss, vSeq);   // Read virus chunk sequence

    return t;
}

void loadVirus()
{
    virusfile.open(FASTA1);
    if (!virusfile.is_open())
    {
        std::cerr << "Error: Cannot open virus file " << FASTA1 << std::endl;
        return;
    }

    string line;
    while (getline(virusfile, line))
    {
        if (line[0] == '>')
        {
            virusdata.chrom = line;
            continue;
        }
        virusdata.seq += line;
    }
    virusdata.start = 0;
    cout << "-loaded virus data- size: " << virusdata.seq.size() << endl;

    // Close the file after use
    virusfile.close();
}

void makeVirusChunks(size_t win, size_t over)
{
    for (size_t p = 0; p + win <= virusdata.seq.size(); p += (win - over))
        vchunks.push_back({p, virusdata.seq.substr(p, win)});
    cout << "-split virus into " << vchunks.size() << " chunks\n";
}