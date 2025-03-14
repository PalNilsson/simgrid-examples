#include <simgrid/s4u.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility> // for std::pair

XBT_LOG_NEW_DEFAULT_CATEGORY(simgrid_example, "SimGrid Job Scheduler Example");

using namespace simgrid::s4u;
using namespace std;
using json = nlohmann::json;

// Global counters for job outcomes.
static int g_total_success = 0;
static unordered_map<int,int> g_error_counts; // maps error_code -> count
static mutex g_mutex;  // For thread-safe updates, if needed.

// A simple Job structure with an error_code.
struct Job {
    string name;
    double load;      // Total simulated processing time required.
    int error_code;   // 0 means success; nonzero (e.g., -1) indicates an error.
    Job(const string &n, double l) : name(n), load(l), error_code(0) {}
};

// Use a constant for the max number of workers
const int MAX_WORKERS = 20;

// Use --mute to suppress verbose output (setting XBT_LOG_DEFAULT_LEVEL does not work to suppress messages since XBT_LOG_NEW_DEFAULT_CATEGORY has already been called)
bool muted = false;


// Function to parse command-line arguments
// Returns a tuple with (input_file, n, queue_name)
tuple<string, int, string> parseArguments(int argc, char* argv[]) {
    unordered_map<string, string> args;

    // Loop over all arguments.
    for (int i = 1; i < argc; i++) {
        string key = argv[i];
        if (key == "--mute") {
            muted = true;
            continue;
        }
        // These options require a value.
        if (key == "--input" || key == "--n" || key == "--queue") {
            if (i + 1 >= argc) {
                throw runtime_error("Error: Missing value for " + key);
            }
            string value = argv[i + 1];
            args[key] = value;
            i++; // Skip the value argument.
        }
    }

    // Validate required arguments.
    if (args.find("--input") == args.end()) {
        throw runtime_error("Error: Missing --input argument.");
    }
    if (args.find("--n") == args.end()) {
        throw runtime_error("Error: Missing --n argument.");
    }
    if (args.find("--queue") == args.end()) {
        throw runtime_error("Error: Missing --queue argument.");
    }

    string input_file = args["--input"];
    string queue_name = args["--queue"];
    int n;
    try {
        n = stoi(args["--n"]);
    } catch (const invalid_argument& e) {
        throw runtime_error("Error: Invalid value for --n. It must be an integer.");
    } catch (const out_of_range& e) {
        throw runtime_error("Error: Value for --n is out of range.");
    }

    return {input_file, n, queue_name};
}


// Worker actor: processes jobs and terminates when receiving a termination message.
void worker() {

    if (!muted) {
        XBT_INFO("Worker %s: Starting", this_actor::get_name().c_str());
    }

    Mailbox* mbox = Mailbox::by_name(this_actor::get_name());
    while (true) {
        Job* job = mbox->get<Job>();
        // Termination signal: if the job name is "exit", break out of the loop.
        if (job->name == "exit") {
            if (!muted) {
                XBT_INFO("Worker %s: Received termination signal. Exiting.", this_actor::get_name().c_str());
            }
            delete job;
            break;
        }
        if (!muted) {
            XBT_INFO("Worker %s: Received job %s with load %f",
                     this_actor::get_name().c_str(), job->name.c_str(), job->load);
        }
        double elapsed = 0.0;
        double slice = 0.1;  // Process in increments of 0.1 seconds.
        while (elapsed < job->load) {
            if (elapsed >= 10.0) {
                if (!muted) {
                    XBT_WARN("Worker %s: Aborting job %s after 10 seconds",
                             this_actor::get_name().c_str(), job->name.c_str());
                }
                job->error_code = -1;
                break;
            }
            double remaining = job->load - elapsed;
            double sleep_time = min(slice, remaining);
            this_actor::sleep_for(sleep_time);
            elapsed += sleep_time;
        }
        
        if (!muted) {
            if (job->error_code == 0) {
                XBT_INFO("Worker %s: Completed job %s in %f seconds", 
                         this_actor::get_name().c_str(), job->name.c_str(), elapsed);
            } else {
                XBT_INFO("Worker %s: Job %s finished with error code %d", 
                         this_actor::get_name().c_str(), job->name.c_str(), job->error_code);
            }
        }
        // Update global summary counters.
        {
            lock_guard<mutex> lock(g_mutex);
            if (job->error_code == 0)
                ++g_total_success;
            else
                g_error_counts[job->error_code]++;
        }
        delete job;
    }
}


// Master actor: creates and sends jobs, then sends termination messages.
void master(int num_jobs) {

    if (!muted) {
        XBT_INFO("Master: Starting");
    }
    for (int i = 0; i < num_jobs; i++) {
        // Generate a job load between 1 and 15 seconds.
        double job_time = 1.0 + (static_cast<double>(rand()) / RAND_MAX) * 14.0;
        Job* job = new Job("job" + to_string(i), job_time);
        // Round-robin assignment: send to one of the workers.
        string worker_name = "worker" + to_string(i % MAX_WORKERS);
        Mailbox::by_name(worker_name)->put(job, sizeof(Job));
        if (!muted) {
            XBT_INFO("Master: Sent job %s with load %f to %s", 
                     job->name.c_str(), job->load, worker_name.c_str());
        }
    }

    // Send termination messages (a "poison pill") to each worker.
    for (int i = 0; i < MAX_WORKERS; i++) {
        string worker_name = "worker" + to_string(i);
        Job* term_job = new Job("exit", 0.0);
        Mailbox::by_name(worker_name)->put(term_job, sizeof(Job));
        if (!muted) {
            XBT_INFO("Master: Sent termination signal to %s", worker_name.c_str());
        }
    }
}


int main(int argc, char* argv[]) {

    // Read input file from arguments --input
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " --input <input error file> --queue <queue name> --n <number of jobs>\n";
        return 1;
    }

    string input_file{""};
    string queue_name{""};
    int total_jobs{0};
    try {
        // Assign values from function
        tie(input_file, total_jobs, queue_name) = parseArguments(argc, argv);
        cout << "Input File: " << input_file << endl;
        cout << "Number of jobs: " << total_jobs << endl;
        cout << "Queue Name: " << queue_name << endl;
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    // Read error codes from JSON file using the input argument
    ifstream file(input_file);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << input_file << endl;
        return EXIT_FAILURE;
    }
    json j;
    file >> j;
    if (!file) {
        cerr << "Error: Failed to parse " << input_file << endl;
        return EXIT_FAILURE;
    }

    // Convert the JSON data to a dictionary
    map<string, map<string, int>> dictionary;

    for (const auto& [site_name, codes] : j.items()) {
        for (const auto& [code, count] : codes.items()) {
            dictionary[site_name][code] = count;
        }
    }

    // Extract error codes and counts for the target site
    unordered_map<string, int> error_codes;
    if (dictionary.count(queue_name) > 0) {
        for (const auto& [code, count] : dictionary[queue_name]) {
            error_codes[code] = count;
        }
    } else {
        cout << "Site not found: " << queue_name << endl;
    }

    // Calculate the total weight
    int total_weight = 0;
    for (const auto& pair : error_codes) {
        total_weight += pair.second;
    }

    // Create a random number engine (Mersenne Twister with a state size of 19937 bits, seeded with a random number)
    random_device rd;
    mt19937 gen(rd());

    // Create a discrete distribution
    vector<double> weights;
    for (const auto& pair : error_codes) {
        weights.push_back(pair.second);
    }
    discrete_distribution<> dist(weights.begin(), weights.end());

    int random_index{0};
    int error_code{0};
    string random_error_code{""};
    auto it = error_codes.cbegin();
    unordered_map<int, int> errorCounts;



    // Initialize the SimGrid endgine
    Engine e(&argc, argv);
    e.load_platform("platform.xml");

    // Create the master actor on host "worker0", passing num_jobs via a lambda.
    Actor::create("master", Host::by_name("worker0"), [total_jobs]() { master(total_jobs); });
    
    // Create some worker actors, each bound to its corresponding host.
    for (int i = 0; i < MAX_WORKERS; i++) {
        string host_name = "worker" + to_string(i);
        Actor::create(host_name, Host::by_name(host_name), worker);
    }

    e.run();

    // After simulation run is finished, print a summary.
    cout << "\n=== Simulation Summary ===" << endl;
    int total_success = g_total_success;
    int total_failures = total_jobs - total_success;
    cout << "Total jobs: " << total_jobs << endl;
    cout << "Successful jobs: " << total_success << endl;
    cout << "Failed jobs: " << total_failures << endl;
    if (total_failures > 0) {
        cout << "Failure details:" << endl;
        for (const auto& kv : g_error_counts) {
            cout << "  Error code " << kv.first << ": " << kv.second << endl;
        }
    }
    cout << "==========================\n" << endl;

    return 0;
}
