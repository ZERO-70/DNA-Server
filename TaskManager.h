#ifndef FASTA_PARSER_H
#define FASTA_PARSER_H
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <fstream> // Add this for ifstream
#include <memory> // For shared_ptr

using namespace std;

// Forward declarations
struct VChunk;
struct Job;
struct Task;
class Socket; // Forward declaration for Socket class

// Structure definitions
struct VChunk
{
    size_t vStart;
    string seq;
};

struct Job
{
    string chrom;
    size_t start;
    string seq;

    string serialize() const;
    static Job deserialize(const string &data);
};

// Global variables - changed to extern declarations
extern std::mutex jobs_mutex;
extern std::ifstream virusfile;
extern Job virusdata;
extern std::queue<Task> taskQ;
extern std::vector<VChunk> vchunks;
extern std::vector<std::shared_ptr<Socket>> socketRefs; // Thread-safe vector for socket references

struct Task
{
    Job human;     // 200 kb human slice
    size_t vIndex; // which virus‑chunk to use

    string serialize() const;
    static Task deserialize(const std::string &data);
};

// Function declarations
bool nextJob(Job &j);
bool loadTasks();
int preparejobs();
void loadVirus();
void makeVirusChunks(size_t win = 5000, size_t over = 1000);

#endif // FASTA_PARSER_H
