// File: cmpe412_multiserver_sim.cpp
// CMPE412 – Multi-Server Queue Simulation (Term Project 01 & 02)
// Authors: Mai Bakr, Mariam Nauman, volt
// Course: Computer Simulation (Fall 2025) – Kadir Has University
// Compile: g++ -std=c++17 cmpe412_multiserver_sim.cpp -O2 -o sim
// Run: ./sim

#include <bits/stdc++.h>
using namespace std;

// ------------------- Configuration -------------------
int NUM_SERVERS = 3;                // base case: 3 servers
int SIM_TIME_MINUTES = 600;         // total simulation time (10 hours)
int MAX_CUSTOMERS_TO_GENERATE = 1000; // safety cap

// Interarrival distribution (discrete, 2–6 minutes)
vector<int> INTERARRIVAL_VALUES = {2, 3, 4, 5, 6};
vector<double> INTERARRIVAL_PROBS = {0.15, 0.25, 0.30, 0.20, 0.10}; // avg ~3.85

// Service time distribution (discrete, 3–8 minutes)
vector<int> SERVICE_VALUES = {3, 4, 5, 6, 7, 8};
vector<double> SERVICE_PROBS = {0.10, 0.20, 0.30, 0.25, 0.10, 0.05}; // avg ~5.25

// Log and output file names
string LOG_FILENAME = "simulation_log.txt";
string STATS_CSV = "stats_per_minute.csv";
string SUMMARY_CSV = "summary_statistics.csv";

// ------------------- Data structures -------------------
struct Event {
    enum Type { ARRIVAL, DEPARTURE } type;
    int time;       // minute when event occurs
    int customerID;
    int serverID;   // used only for DEPARTURE
};

struct EventCmp {
    bool operator()(const Event& a, const Event& b) const { return a.time > b.time; }
};

struct Customer {
    int id;
    int arrivalTime;
    int serviceStartTime;
    int serviceTime;
};

struct Server {
    bool busy = false;
    int currentCustomer = -1;
    int serviceEndTime = -1;
    long long busyTime = 0;
};

// ------------------- RNG helpers -------------------
static mt19937 rng((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());

int draw_from_discrete(const vector<int>& values, const vector<double>& probs) {
    double r = generate_canonical<double, 10>(rng);
    double cum = 0.0;
    for (size_t i = 0; i < values.size(); ++i) {
        cum += probs[i];
        if (r <= cum) return values[i];
    }
    return values.back();
}

// ------------------- Main simulation -------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ofstream logf(LOG_FILENAME);
    ofstream statsf(STATS_CSV);
    ofstream summaryf(SUMMARY_CSV);

    statsf << "minute,queue_length,arrivals_this_min,departures_this_min,servers_busy,total_in_system\n";

    // Initialize data
    priority_queue<Event, vector<Event>, EventCmp> FEL;
    vector<Server> servers(NUM_SERVERS);
    queue<Customer> waitingQueue;
    unordered_map<int, Customer> customerDB;

    // Pre-generate arrivals
    int time_cursor = 0;
    int cust_id = 0;
    while (time_cursor <= SIM_TIME_MINUTES && cust_id < MAX_CUSTOMERS_TO_GENERATE) {
        int inter = draw_from_discrete(INTERARRIVAL_VALUES, INTERARRIVAL_PROBS);
        time_cursor += inter;
        if (time_cursor > SIM_TIME_MINUTES) break;
        Event ev{Event::ARRIVAL, time_cursor, cust_id, -1};
        FEL.push(ev);
        Customer c{cust_id, time_cursor, -1, -1};
        customerDB[cust_id] = c;
        cust_id++;
    }

    int totalCustomersGenerated = cust_id;
    logf << "Simulation start: sim_time=" << SIM_TIME_MINUTES
         << " minutes, servers=" << NUM_SERVERS << "\n";
    logf << "Pre-generated " << totalCustomersGenerated << " arrivals\n";

    // Statistics
    long long totalWaitingTime = 0;
    long long totalServiceTime = 0;
    int totalServed = 0;
    long long queueLengthAccumulator = 0;

    // Helper: assign server to a customer
    auto assign_server_to_customer = [&](int serverIdx, Customer& cust, int now) {
        int serviceT = draw_from_discrete(SERVICE_VALUES, SERVICE_PROBS);
        cust.serviceStartTime = now;
        cust.serviceTime = serviceT;
        servers[serverIdx].busy = true;
        servers[serverIdx].currentCustomer = cust.id;
        servers[serverIdx].serviceEndTime = now + serviceT;
        Event dep{Event::DEPARTURE, now + serviceT, cust.id, serverIdx};
        FEL.push(dep);
        logf << "minute " << now << ": customer " << cust.id
             << " assigned to server " << serverIdx
             << " service=" << serviceT << " end=" << (now + serviceT) << "\n";
    };

    // Main simulation loop (minute by minute)
    for (int minute = 0; minute <= SIM_TIME_MINUTES; ++minute) {
        int arrivals_this_min = 0;
        int departures_this_min = 0;

        // 1) Handle departures
        while (!FEL.empty() && FEL.top().time == minute && FEL.top().type == Event::DEPARTURE) {
            Event ev = FEL.top();
            FEL.pop();
            int sid = ev.serverID;
            int cid = ev.customerID;
            servers[sid].busy = false;
            servers[sid].currentCustomer = -1;
            servers[sid].serviceEndTime = -1;
            departures_this_min++;

            Customer& c = customerDB[cid];
            int wait = c.serviceStartTime - c.arrivalTime;
            totalWaitingTime += wait;
            totalServiceTime += c.serviceTime;
            totalServed++;

            logf << "minute " << minute << ": customer " << cid
                 << " departed from server " << sid
                 << " waited=" << wait
                 << " service=" << c.serviceTime << "\n";
        }

        // 2) Assign waiting customers to idle servers
        for (int s = 0; s < NUM_SERVERS; ++s) {
            if (!waitingQueue.empty() && !servers[s].busy) {
                Customer c = waitingQueue.front();
                waitingQueue.pop();
                assign_server_to_customer(s, c, minute);
                customerDB[c.id] = c;
            }
        }

        // 3) Process new arrivals
        while (!FEL.empty() && FEL.top().time == minute && FEL.top().type == Event::ARRIVAL) {
            Event ev = FEL.top();
            FEL.pop();
            int cid = ev.customerID;
            arrivals_this_min++;
            Customer& c = customerDB[cid];
            logf << "minute " << minute << ": customer " << cid << " arrived\n";
            bool assigned = false;
            for (int s = 0; s < NUM_SERVERS; ++s) {
                if (!servers[s].busy) {
                    assign_server_to_customer(s, c, minute);
                    customerDB[cid] = c;
                    assigned = true;
                    break;
                }
            }
            if (!assigned) {
                waitingQueue.push(c);
                logf << "minute " << minute << ": customer " << cid
                     << " queued (position=" << waitingQueue.size() << ")\n";
            }
        }

        // 4) Update server busy times
        int busyCount = 0;
        for (int s = 0; s < NUM_SERVERS; ++s) {
            if (servers[s].busy) {
                servers[s].busyTime++;
                busyCount++;
            }
        }

        // 5) Track queue length
        queueLengthAccumulator += (long long)waitingQueue.size();

        int totalInSystem = waitingQueue.size();
        for (int s = 0; s < NUM_SERVERS; ++s)
            if (servers[s].busy) totalInSystem++;

        statsf << minute << "," << waitingQueue.size() << "," << arrivals_this_min << ","
               << departures_this_min << "," << busyCount << "," << totalInSystem << "\n";

        // 6) Optional early stop
        if (FEL.empty() && waitingQueue.empty()) {
            bool anyBusy = false;
            for (auto& sv : servers)
                if (sv.busy) { anyBusy = true; break; }
            if (!anyBusy) {
                logf << "No more events and all servers idle at minute "
                     << minute << ", terminating early.\n";
            }
        }
    }

    // ------------------- Final statistics -------------------
    double avgWaiting = totalServed ? (double)totalWaitingTime / totalServed : 0.0;
    double avgService = totalServed ? (double)totalServiceTime / totalServed : 0.0;
    double avgQueueLen = (double)queueLengthAccumulator / (SIM_TIME_MINUTES + 1);
    double throughput = (double)totalServed / (SIM_TIME_MINUTES + 1);

    logf << "\n--- SUMMARY ---\n";
    logf << "Total served = " << totalServed << "\n";
    logf << "Average waiting time = " << avgWaiting << " minutes\n";
    logf << "Average service time = " << avgService << " minutes\n";
    logf << "Average queue length = " << avgQueueLen << "\n";
    logf << "Throughput (served/min) = " << throughput << "\n";

    // Server utilizations
    summaryf << "server_id,busy_time,utilization\n";
    for (int s = 0; s < NUM_SERVERS; ++s) {
        double util = (double)servers[s].busyTime / (SIM_TIME_MINUTES + 1);
        summaryf << s << "," << servers[s].busyTime << "," << util << "\n";
        logf << "Server " << s << " busy_time=" << servers[s].busyTime
             << " utilization=" << util << "\n";
    }

    // Final metrics summary
    summaryf << "metric,value\n";
    summaryf << "total_served," << totalServed << "\n";
    summaryf << "avg_waiting," << avgWaiting << "\n";
    summaryf << "avg_service," << avgService << "\n";
    summaryf << "avg_queue_len," << avgQueueLen << "\n";
    summaryf << "throughput," << throughput << "\n";

    logf << "Simulation complete.\n";
    logf << "Logs: " << LOG_FILENAME
         << ", per-minute stats: " << STATS_CSV
         << ", summary: " << SUMMARY_CSV << "\n";

    logf.close();
    statsf.close();
    summaryf.close();
    return 0;
}
