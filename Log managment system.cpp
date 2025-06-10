#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

// ====================== CUSTOM DATA STRUCTURES ======================

// Custom Min-Heap for priority-based log processing
template<typename T, typename Compare = less<T>>
class PriorityLogQueue {
private:
    vector<T> heap;
    Compare comp;
    mutex mtx;

    void heapify_up(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (comp(heap[parent], heap[index])) break;
            swap(heap[parent], heap[index]);
            index = parent;
        }
    }

    void heapify_down(int index) {
        int left, right, largest;
        while (true) {
            left = 2 * index + 1;
            right = 2 * index + 2;
            largest = index;

            if (left < heap.size() && comp(heap[left], heap[largest]))
                largest = left;
            if (right < heap.size() && comp(heap[right], heap[largest]))
                largest = right;
            if (largest == index) break;

            swap(heap[index], heap[largest]);
            index = largest;
        }
    }

public:
    void push(const T& value) {
        lock_guard<mutex> lock(mtx);
        heap.push_back(value);
        heapify_up(heap.size() - 1);
    }

    T pop() {
        lock_guard<mutex> lock(mtx);
        T top = heap.front();
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) heapify_down(0);
        return top;
    }

    bool empty() const {
        return heap.empty();
    }

    size_t size() const {
        return heap.size();
    }
};

// Custom Graph for log relationship analysis
class LogRelationshipGraph {
private:
    unordered_map<string, vector<pair<string, int>>> adj_list;
    mutex mtx;

public:
    void add_relationship(const string& src, const string& dest, int weight = 1) {
        lock_guard<mutex> lock(mtx);
        adj_list[src].emplace_back(dest, weight);
        adj_list[dest]; // Ensure destination exists in map
    }

    vector<pair<string, int>> get_related_logs(const string& log_type) {
        lock_guard<mutex> lock(mtx);
        return adj_list[log_type];
    }

    void print_top_relationships(int count = 5) {
        lock_guard<mutex> lock(mtx);
        PriorityLogQueue<pair<string, int>, greater<pair<string, int>>> pq;

        for (const auto& entry : adj_list) {
            int total_weight = 0;
            for (const auto& rel : entry.second) {
                total_weight += rel.second;
            }
            pq.push({ total_weight, entry.first });
        }

        cout << "Top " << count << " Log Relationships:\n";
        for (int i = 0; i < count && !pq.empty(); ++i) {
            auto item = pq.pop();
            cout << i + 1 << ". " << item.second << " (weight: " << item.first << ")\n";
        }
    }
};

// ====================== LOG MANAGEMENT SYSTEM ======================

class LogManager {
private:
    struct LogProcess {
        pid_t pid;
        string program_name;
        string log_file;
        time_t start_time;
        thread worker_thread;
    };

    unordered_map<pid_t, LogProcess> active_processes;
    LogRelationshipGraph log_graph;
    mutex process_mutex;
    string log_directory = "./logs";

    // Bloom Filter for fast log type checking
    class LogTypeFilter {
    private:
        vector<bool> bits;
        const int size = 1000;
        mutex mtx;

        size_t hash1(const string& s) const {
            hash<string> hasher;
            return hasher(s) % size;
        }

        size_t hash2(const string& s) const {
            size_t h = 0;
            for (char c : s) h = (h * 31 + c) % size;
            return h;
        }

    public:
        LogTypeFilter() : bits(size, false) {}

        void add(const string& log_type) {
            lock_guard<mutex> lock(mtx);
            bits[hash1(log_type)] = true;
            bits[hash2(log_type)] = true;
        }

        bool might_contain(const string& log_type) const {
            lock_guard<mutex> lock(mtx);
            return bits[hash1(log_type)] && bits[hash2(log_type)];
        }
    } log_type_filter;

    void monitor_process(LogProcess& lp) {
        ofstream log_file(lp.log_file, ios::app);
        if (!log_file) {
            cerr << "Failed to open log file for " << lp.program_name << endl;
            return;
        }

        string command = "strace -p " + to_string(lp.pid) + " -f -e trace=write -o /dev/stdout 2>&1";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return;

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            string log_entry(buffer);
            time_t now = time(nullptr);

            json log_json = {
                {"pid", lp.pid},
                {"program", lp.program_name},
                {"timestamp", now},
                {"entry", log_entry},
                {"type", "system_call"}
            };

            log_file << log_json.dump() << "\n";
            log_type_filter.add("system_call");
            log_graph.add_relationship(lp.program_name, "system_call");
        }
        pclose(pipe);
    }

    vector<json> load_log_file(const string& filename) {
        vector<json> logs;
        ifstream file(filename);
        string line;

        while (getline(file, line)) {
            try {
                logs.push_back(json::parse(line));
            }
            catch (...) {
                cerr << "Failed to parse log entry: " << line << endl;
            }
        }
        return logs;
    }

    vector<string> get_all_log_files() {
        vector<string> files;
        DIR* dir;
        struct dirent* ent;

        if ((dir = opendir(log_directory.c_str())) != nullptr) {
            while ((ent = readdir(dir)) != nullptr) {
                string filename = ent->d_name;
                if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".log") {
                    files.push_back(log_directory + "/" + filename);
                }
            }
            closedir(dir);
        }
        return files;
    }

public:
    LogManager() {
        // Create log directory if it doesn't exist
        struct stat st;
        if (stat(log_directory.c_str(), &st) == -1) {
            mkdir(log_directory.c_str(), 0700);
        }
    }

    void start_logging(pid_t pid, const string& program_name) {
        lock_guard<mutex> lock(process_mutex);
        if (active_processes.count(pid)) {
            cout << "Already logging process " << pid << endl;
            return;
        }

        string log_file = log_directory + "/" + program_name + "_" + to_string(pid) + ".log";
        LogProcess lp{ pid, program_name, log_file, time(nullptr) };
        lp.worker_thread = thread(&LogManager::monitor_process, this, ref(lp));
        active_processes.emplace(pid, move(lp));

        cout << "Started logging for PID " << pid << " (" << program_name << ")\n";
        cout << "Log file: " << log_file << endl;
    }

    void stop_logging(pid_t pid) {
        lock_guard<mutex> lock(process_mutex);
        auto it = active_processes.find(pid);
        if (it == active_processes.end()) {
            cout << "No active logging for PID " << pid << endl;
            return;
        }

        kill(pid, SIGCONT); // Ensure process is running to detach strace
        it->second.worker_thread.detach();
        active_processes.erase(it);

        cout << "Stopped logging for PID " << pid << endl;
    }

    void list_active_logs() {
        lock_guard<mutex> lock(process_mutex);
        if (active_processes.empty()) {
            cout << "No active log processes\n";
            return;
        }

        cout << "Active Log Processes:\n";
        for (const auto& [pid, lp] : active_processes) {
            cout << "PID: " << pid << "\tProgram: " << lp.program_name
                << "\tLog File: " << lp.log_file << endl;
        }
    }

    void analyze_logs(pid_t pid) {
        string log_file;
        {
            lock_guard<mutex> lock(process_mutex);
            auto it = active_processes.find(pid);
            if (it == active_processes.end()) {
                cout << "No active logging for PID " << pid << endl;
                return;
            }
            log_file = it->second.log_file;
        }

        vector<json> logs = load_log_file(log_file);
        if (logs.empty()) {
            cout << "No logs found for PID " << pid << endl;
            return;
        }

        cout << "Loaded " << logs.size() << " log entries\n";
        cout << "Enter analysis commands (type 'help' for options):\n";

        string command;
        while (true) {
            cout << "log-analyzer> ";
            getline(cin, command);

            if (command == "exit") break;
            if (command == "help") {
                cout << "Available commands:\n"
                    << "!!errors - Show all error messages\n"
                    << "!!stats - Show log statistics\n"
                    << "!!timeline - Show timeline of events\n"
                    << "!!search <query> - Search for specific text\n"
                    << "!!relationships - Show log relationships\n"
                    << "exit - Exit analysis mode\n";
                continue;
            }

            if (command == "!!errors") {
                cout << "Error messages:\n";
                for (const auto& log : logs) {
                    if (log["entry"].get<string>().find("error") != string::npos ||
                        log["entry"].get<string>().find("Error") != string::npos ||
                        log["entry"].get<string>().find("ERROR") != string::npos) {
                        cout << log["timestamp"].get<time_t>(A) << ": "
                            << log["entry"].get<string>();
                    }
                }
            }
            else if (command == "!!stats") {
                unordered_map<string, int> type_counts;
                for (const auto& log : logs) {
                    type_counts[log["type"].get<string>()]++;
                }

                cout << "Log Statistics:\n";
                for (const auto& [type, count] : type_counts) {
                    cout << type << ": " << count << " entries\n";
                }
            }
            else if (command == "!!timeline") {
                sort(logs.begin(), logs.end(), [](const json& a, const json& b) {
                    return a["timestamp"].get<time_t>() < b["timestamp"].get<time_t>();
                    });

                cout << "Timeline:\n";
                for (const auto& log : logs) {
                    time_t ts = log["timestamp"].get<time_t>();
                    cout << ctime(&ts) << ": " << log["type"].get<string>() << " - "
                        << log["entry"].get<string>().substr(0, 50) << "...\n";
                }
            }
            else if (command.find("!!search ") == 0) {
                string query = command.substr(9);
                cout << "Search results for '" << query << "':\n";
                for (const auto& log : logs) {
                    if (log["entry"].get<string>().find(query) != string::npos) {
                        cout << log["timestamp"].get<time_t>() << ": "
                            << log["entry"].get<string>();
                    }
                }
            }
            else if (command == "!!relationships") {
                log_graph.print_top_relationships();
            }
            else {
                cout << "Unknown command. Type 'help' for options.\n";
            }
        }
    }

    void show_system_processes() {
        cout << "System Processes:\n";
        system("ps aux | head -n 10"); // Show first 10 processes
    }
};

// ====================== MENU INTERFACE ======================

void display_menu() {
    cout << "\nLog Management System\n"
        << "1. Start Log Capture\n"
        << "2. Stop Log Capture\n"
        << "3. List Active Logs\n"
        << "4. Analyze Logs\n"
        << "5. Show System Processes\n"
        << "6. Exit\n"
        << "Enter choice: ";
}

int main() {
    LogManager log_manager;
    int choice;
    pid_t pid;

    while (true) {
        display_menu();
        cin >> choice;
        cin.ignore(); // Clear newline

        switch (choice) {
        case 1: {
            log_manager.show_system_processes();
            cout << "Enter PID to monitor: ";
            cin >> pid;
            cin.ignore();

            string program_name;
            cout << "Enter program name: ";
            getline(cin, program_name);

            log_manager.start_logging(pid, program_name);
            break;
        }
        case 2: {
            cout << "Enter PID to stop monitoring: ";
            cin >> pid;
            cin.ignore();
            log_manager.stop_logging(pid);
            break;
        }
        case 3:
            log_manager.list_active_logs();
            break;
        case 4: {
            cout << "Enter PID to analyze: ";
            cin >> pid;
            cin.ignore();
            log_manager.analyze_logs(pid);
            break;
        }
        case 5:
            log_manager.show_system_processes();
            break;
        case 6:
            return 0;
        default:
            cout << "Invalid choice\n";
        }
    }
}