#include <bits/stdc++.h>
using namespace std;

int serverNumbers = 3;
int durationSimu = 600;
int ultimateCustomers = 1000;

vector<int> interarrivalVals = {2, 3, 4, 5, 6};
vector<double> interarrivalProb = {0.15, 0.25, 0.30, 0.20, 0.10};

vector<int> serviceVals = {3, 4, 5, 6, 7, 8};
vector<double> serviceProbs = {0.10, 0.20, 0.30, 0.25, 0.10, 0.05};

string simuFile = "simulationFile.txt";
string statsFile = "statsFile.csv";
string summaryFile = "summaryFile.csv";

struct Event {
    enum Type { ARRIVAL, DEPARTURE } type;
    int time;
    int customerID;
    int serverID;
};
struct EventCmp { bool operator()(const Event& a, const Event& b) const { return a.time > b.time; } };

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

static mt19937 rng((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());
int discreteDraw(const vector<int>& values, const vector<double>& probs) {
    double r = generate_canonical<double, 10>(rng);
    double cum = 0.0;
    for (size_t i = 0; i < values.size(); ++i) {
        cum += probs[i];
        if (r <= cum) return values[i];
    }
    return values.back();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ofstream logf(simuFile);
    ofstream statsf(statsFile);
    ofstream summaryf(summaryFile);

    statsf << "minute,queue_length,arrivalsNow,departuresNow,servers_busy,total_in_system\n";

    priority_queue<Event, vector<Event>, EventCmp> FEL;
    vector<Server> servers(serverNumbers);
    queue<Customer> waitingQueue;
    unordered_map<int, Customer> customerDB;

    int arrivalEvents = 0, customerIDS = 0;
    while (arrivalEvents <= durationSimu && customerIDS < ultimateCustomers) {
        int inter = discreteDraw(interarrivalVals, interarrivalProb);
        arrivalEvents += inter;
        if (arrivalEvents > durationSimu) break;
        Event ev{Event::ARRIVAL, arrivalEvents, customerIDS, -1};
        FEL.push(ev);
        Customer c{customerIDS, arrivalEvents, -1, -1};
        customerDB[customerIDS] = c;
        customerIDS++;
    }

    int allCustomers = customerIDS;
    logf << "Simulation benining: " << durationSimu
         << " minutes Servers=" << serverNumbers << "\n";
    logf << "Number of simulated customers: " << allCustomers << " arrivals\n\n";


    logf << left << setw(8) << "Minute"
         << setw(12) << "Event"
         << setw(10) << "CustID"
         << setw(10) << "Server"
         << setw(14) << "ServiceTime"
         << setw(12) << "Details" << "\n";
    logf << string(66, '-') << "\n";

    long long waitTime = 0, servTime = 0, queueLength = 0;
    int totalServed = 0;

    auto customerServer = [&](int serverIdx, Customer& cust, int now) {
        int serviceT = discreteDraw(serviceVals, serviceProbs);
        cust.serviceStartTime = now;
        cust.serviceTime = serviceT;
        servers[serverIdx].busy = true;
        servers[serverIdx].currentCustomer = cust.id;
        servers[serverIdx].serviceEndTime = now + serviceT;
        FEL.push({Event::DEPARTURE, now + serviceT, cust.id, serverIdx});
        logf << left << setw(8) << now
             << setw(12) << "ASSIGNED"
             << setw(10) << cust.id
             << setw(10) << serverIdx
             << setw(14) << serviceT
             << setw(12) << ("End=" + to_string(now + serviceT)) << "\n";
    };

    int minute = 0;
    while (!FEL.empty() && minute <= durationSimu) {
        minute = FEL.top().time; 
        int arrivalsNow = 0;
        int departuresNow = 0;

        while (!FEL.empty() && FEL.top().time == minute && FEL.top().type == Event::DEPARTURE) {
            Event ev = FEL.top(); FEL.pop();
            int sid = ev.serverID;
            int cid = ev.customerID;
            servers[sid].busy = false;
            servers[sid].currentCustomer = -1;
            servers[sid].serviceEndTime = -1;
            departuresNow++;

            Customer& c = customerDB[cid];
            int wait = c.serviceStartTime - c.arrivalTime;
            waitTime += wait;
            servTime += c.serviceTime;
            totalServed++;

            logf << left << setw(8) << minute
                 << setw(12) << "DEPARTURE"
                 << setw(10) << cid
                 << setw(10) << sid
                 << setw(14) << c.serviceTime
                 << setw(12) << ("Wait=" + to_string(wait)) << "\n";
        }

        for (int s = 0; s < serverNumbers; ++s) {
            if (!waitingQueue.empty() && !servers[s].busy) {
                Customer c = waitingQueue.front();
                waitingQueue.pop();
                customerServer(s, c, minute);
                customerDB[c.id] = c;
            }
        }

        while (!FEL.empty() && FEL.top().time == minute && FEL.top().type == Event::ARRIVAL) {
            Event ev = FEL.top(); FEL.pop();
            int cid = ev.customerID;
            arrivalsNow++;
            Customer& c = customerDB[cid];

            logf << left << setw(8) << minute
                 << setw(12) << "ARRIVAL"
                 << setw(10) << cid
                 << setw(10) << "-"
                 << setw(14) << "-"
                 << setw(12) << "" << "\n";

            bool assigned = false;
            for (int s = 0; s < serverNumbers; ++s) {
                if (!servers[s].busy) {
                    customerServer(s, c, minute);
                    customerDB[cid] = c;
                    assigned = true;
                    break;
                }
            }
            if (!assigned) {
                waitingQueue.push(c);
                logf << left << setw(8) << minute
                     << setw(12) << "QUEUED"
                     << setw(10) << cid
                     << setw(10) << "-"
                     << setw(14) << "-"
                     << setw(12) << ("Pos=" + to_string(waitingQueue.size())) << "\n";
            }
        }

        int busyCount = 0;
        for (int s = 0; s < serverNumbers; ++s) {
            if (servers[s].busy) { servers[s].busyTime++; busyCount++; }
        }
        queueLength += waitingQueue.size();

        int sysTotal = waitingQueue.size();
        for (int s = 0; s < serverNumbers; ++s)
            if (servers[s].busy) sysTotal++;

        statsf << minute << "," << waitingQueue.size() << ","
               << arrivalsNow << "," << departuresNow << ","
               << busyCount << "," << sysTotal << "\n";

        if (FEL.empty()) break; 
    }

    logf << string(66, '-') << "\n";

    double avgWaiting = totalServed ? (double)waitTime / totalServed : 0.0;
    double avgService = totalServed ? (double)servTime / totalServed : 0.0;
    double avgQueueLen = (double)queueLength / (durationSimu + 1);
    double outputVal = (double)totalServed / (durationSimu + 1);
    
const string BLUE = "\033[36m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string RESET = "\033[0m";

logf << "\n";
logf << "╔══════════════════════════════════════════════════════╗\n";
logf << "║                   SIMULATION SUMMARY                 ║\n";
logf << "╠══════════════════════════════════════════════════════╣\n";
logf << "   Total served          : " << setw(10) << totalServed    << "\n";
logf << "   Average waiting time  : " << setw(10) << fixed << setprecision(2) << avgWaiting << " minutes         \n";
logf << "   Average service time  : " << setw(10) << avgService     << " minutes         \n";
logf << "   Average queue length  : " << setw(10) << avgQueueLen    << "                     \n";
logf << "   Output value          : " << setw(10) << outputVal      << "                     \n";
logf << "╚══════════════════════════════════════════════════════╝\n";

    summaryf << "server_id,busy_time,utilisation\n";
    for (int s = 0; s < serverNumbers; ++s) {
        double util = (double)servers[s].busyTime / (durationSimu + 1);
        summaryf << s << "," << servers[s].busyTime << "," << util << "\n";
        logf << "Server " << s << " working time=" << servers[s].busyTime
             << " utilisation=" << util << "\n";
    }

    summaryf << "metric,value\n";
    summaryf << "total_served," << totalServed << "\n";
    summaryf << "avg_waiting," << avgWaiting << "\n";
    summaryf << "avg_service," << avgService << "\n";
    summaryf << "avg_queue_len," << avgQueueLen << "\n";
    summaryf << "outputVal," << outputVal << "\n";

    logf.close(); statsf.close(); summaryf.close();
    return 0;
}
