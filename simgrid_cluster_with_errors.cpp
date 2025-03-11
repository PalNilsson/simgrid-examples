#include <simgrid/s4u.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <mutex>

XBT_LOG_NEW_DEFAULT_CATEGORY(simgrid_example, "SimGrid Job Scheduler Example");

using namespace simgrid::s4u;

// Global counters for job outcomes.
static int g_total_success = 0;
static std::unordered_map<int,int> g_error_counts; // maps error_code -> count
static std::mutex g_mutex;  // For thread-safe updates, if needed.

// A simple Job structure with an error_code.
struct Job {
    std::string name;
    double load;      // Total simulated processing time required.
    int error_code;   // 0 means success; nonzero (e.g., -1) indicates an error.
    Job(const std::string &n, double l) : name(n), load(l), error_code(0) {}
};

// Worker actor: processes jobs and terminates when receiving a termination message.
void worker() {
    XBT_INFO("Worker %s: Starting", this_actor::get_name().c_str());
    Mailbox* mbox = Mailbox::by_name(this_actor::get_name());
    while (true) {
        Job* job = mbox->get<Job>();
        // Termination signal: if the job name is "exit", break out of the loop.
        if (job->name == "exit") {
            XBT_INFO("Worker %s: Received termination signal. Exiting.", this_actor::get_name().c_str());
            delete job;
            break;
        }
        XBT_INFO("Worker %s: Received job %s with load %f",
                 this_actor::get_name().c_str(), job->name.c_str(), job->load);
        
        double elapsed = 0.0;
        double slice = 0.1;  // Process in increments of 0.1 seconds.
        while (elapsed < job->load) {
            if (elapsed >= 10.0) {
                XBT_WARN("Worker %s: Aborting job %s after 10 seconds", 
                         this_actor::get_name().c_str(), job->name.c_str());
                job->error_code = -1;
                break;
            }
            double remaining = job->load - elapsed;
            double sleep_time = std::min(slice, remaining);
            this_actor::sleep_for(sleep_time);
            elapsed += sleep_time;
        }
        
        if (job->error_code == 0) {
            XBT_INFO("Worker %s: Completed job %s in %f seconds", 
                     this_actor::get_name().c_str(), job->name.c_str(), elapsed);
        } else {
            XBT_INFO("Worker %s: Job %s finished with error code %d", 
                     this_actor::get_name().c_str(), job->name.c_str(), job->error_code);
        }
        
        // Update global summary counters.
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (job->error_code == 0)
                ++g_total_success;
            else
                g_error_counts[job->error_code]++;
        }
        delete job;
    }
}

// Master actor: creates and sends jobs, then sends termination messages.
void master() {
    XBT_INFO("Master: Starting");
    int num_jobs = 20;  // Total number of jobs.
    for (int i = 0; i < num_jobs; i++) {
        // Generate a job load between 1 and 15 seconds.
        double job_time = 1.0 + (static_cast<double>(rand()) / RAND_MAX) * 14.0;
        Job* job = new Job("job" + std::to_string(i), job_time);
        // Round-robin assignment: send to one of the workers.
        std::string worker_name = "worker" + std::to_string(i % 10);
        Mailbox::by_name(worker_name)->put(job, sizeof(Job));
        XBT_INFO("Master: Sent job %s with load %f to %s", 
                 job->name.c_str(), job->load, worker_name.c_str());
    }
    
    // Send termination messages (a "poison pill") to each worker.
    for (int i = 0; i < 10; i++) {
        std::string worker_name = "worker" + std::to_string(i);
        Job* term_job = new Job("exit", 0.0);
        Mailbox::by_name(worker_name)->put(term_job, sizeof(Job));
        XBT_INFO("Master: Sent termination signal to %s", worker_name.c_str());
    }
}

int main(int argc, char* argv[]) {
    Engine e(&argc, argv);
    e.load_platform("platform.xml");

    // Create the master actor on host "worker0".
    Actor::create("master", Host::by_name("worker0"), master);
    
    // Create 10 worker actors, each bound to its corresponding host.
    for (int i = 0; i < 10; i++) {
        std::string host_name = "worker" + std::to_string(i);
        Actor::create(host_name, Host::by_name(host_name), worker);
    }

    e.run();

    // After simulation run is finished, print a summary.
    std::cout << "\n=== Simulation Summary ===" << std::endl;
    int total_jobs = 20;  // as sent by master
    int total_success = g_total_success;
    int total_failures = total_jobs - total_success;
    std::cout << "Total jobs: " << total_jobs << std::endl;
    std::cout << "Successful jobs: " << total_success << std::endl;
    std::cout << "Failed jobs: " << total_failures << std::endl;
    if (total_failures > 0) {
        std::cout << "Failure details:" << std::endl;
        for (const auto& kv : g_error_counts) {
            std::cout << "  Error code " << kv.first << ": " << kv.second << std::endl;
        }
    }
    std::cout << "==========================\n" << std::endl;

    return 0;
}
