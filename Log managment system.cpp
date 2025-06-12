#define _CRT_SECURE_NO_WARNINGS // Disable deprecation warnings for ctime

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
#include <windows.h>
#include <tlhelp32.h>

using namespace std;
using namespace std::chrono;

// ====================== UTILITY FUNCTIONS ======================

void clearScreen() {
    system("cls");
}

void printColored(const string& text, int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
    cout << text;
    SetConsoleTextAttribute(hConsole, 7); // Reset to default
}

void printHeader(const string& title) {
    clearScreen();
    printColored("\n========================================\n", 10);
    printColored("          " + title + "\n", 12);
    printColored("========================================\n\n", 10);
}

void printMenuOption(int num, const string& text) {
    printColored(" " + to_string(num) + ". ", 11);
    printColored(text + "\n", 15);
}

void printError(const string& message) {
    printColored("[ERROR] " + message + "\n", 12);
}

void printSuccess(const string& message) {
    printColored("[SUCCESS] " + message + "\n", 10);
}

void printWarning(const string& message) {
    printColored("[WARNING] " + message + "\n", 14);
}

string formatTime(time_t time) {
    char buffer[26];
    ctime_s(buffer, sizeof(buffer), &time);
    string timeStr(buffer);
    timeStr.pop_back(); // Remove newline
    return timeStr;
}

// ====================== CUSTOM DATA STRUCTURES ======================

class LogBuffer {
private:
    static const int MAX_CAPACITY = 10000;  // Maximum number of logs to store
    vector<string> buffer;                  // Changed from json array to string vector
    int head = 0;                           // Index of the oldest log
    int tail = 0;                           // Index where next log will be added
    int count = 0;                          // Current number of logs
    mutable std::mutex mtx;                 // Mutex for thread safety

public:
    LogBuffer() : buffer(MAX_CAPACITY) {}   // Initialize buffer with size

    // Add a new log to the buffer
    bool push(const string& log) {
        std::lock_guard<std::mutex> lock(mtx);
        if (count == MAX_CAPACITY) {
            return false;  // Buffer is full
        }

        buffer[tail] = log;
        tail = (tail + 1) % MAX_CAPACITY;
        count++;
        return true;
    }

    // Remove and return the oldest log
    bool pop(string& log) {
        std::lock_guard<std::mutex> lock(mtx);
        if (count == 0) {
            return false;  // Buffer is empty
        }

        log = buffer[head];
        head = (head + 1) % MAX_CAPACITY;
        count--;
        return true;
    }

    // Get all logs as a vector
    vector<string> to_vector() const {
        std::lock_guard<std::mutex> lock(mtx);
        vector<string> logs;
        for (int i = 0; i < count; i++) {
            logs.push_back(buffer[(head + i) % MAX_CAPACITY]);
        }
        return logs;
    }

    // Get the number of logs currently in the buffer
    int size() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return count; 
    }

    // Check if the buffer is empty
    bool empty() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0; 
    }

    // Check if the buffer is full
    bool full() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return count == MAX_CAPACITY; 
    }

    // Clear all logs from the buffer
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        head = 0;
        tail = 0;
        count = 0;
    }
};

class LogSearchIndex {
private:
    struct TrieNode {
        unordered_map<char, shared_ptr<TrieNode>> children;
        vector<size_t> log_indices;
    };

    shared_ptr<TrieNode> root;
    vector<string>* log_store;  // Changed from vector<json> to vector<string>
    mutable std::mutex mtx;

public:
    explicit LogSearchIndex(vector<string>& store) : log_store(&store), root(make_shared<TrieNode>()) {}

    void insert(const string& text, size_t log_index) {
        std::lock_guard<std::mutex> lock(mtx);
        auto node = root;
        for (char ch : text) {
            if (!node->children[ch]) {
                node->children[ch] = make_shared<TrieNode>();
            }
            node = node->children[ch];
            node->log_indices.push_back(log_index);
        }
    }

    vector<string> search(const string& query) {
        std::lock_guard<std::mutex> lock(mtx);
        auto node = root;

        for (char ch : query) {
            if (!node->children[ch]) {
                return {};
            }
            node = node->children[ch];
        }

        vector<string> results;
        queue<shared_ptr<TrieNode>> q;
        q.push(node);

        while (!q.empty()) {
            auto current = q.front();
            q.pop();

            for (size_t index : current->log_indices) {
                if (index < log_store->size()) {
                    results.push_back((*log_store)[index]);
                }
            }

            for (auto& child_pair : current->children) {
                q.push(child_pair.second);
            }
        }

        return results;
    }
};

// ====================== WINDOWS PROCESS UTILITIES ======================

vector<pair<DWORD, string>> getRunningProcesses() {
    vector<pair<DWORD, string>> processes;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        printError("Failed to create process snapshot");
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        printError("Failed to get first process");
        return processes;
    }

    do {
        wstring wideName(pe32.szExeFile);
        string processName(wideName.begin(), wideName.end());
        processes.emplace_back(pe32.th32ProcessID, processName);
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return processes;
}

string getProcessName(DWORD pid) {
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return "Unknown";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "Unknown";
    }

    do {
        if (pe32.th32ProcessID == pid) {
            wstring wideName(pe32.szExeFile);
            string processName;
            processName.reserve(wideName.length());
            for (wchar_t wc : wideName) {
                processName += static_cast<char>(wc & 0xFF);  // Safe conversion
            }
            CloseHandle(hProcessSnap);
            return processName;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return "Unknown";
}

// ====================== LOG MANAGER CLASS ======================

class LogManager {
private:
    struct LogProcess {
        DWORD pid;
        string program_name;
        string log_file;
        time_t start_time;
        thread worker_thread;
        bool running = true;

        LogProcess(DWORD p, const string& name, const string& file)
            : pid(p), program_name(name), log_file(file), start_time(time(nullptr)) {
        }
    };

    vector<string> all_logs;  // Changed from vector<json> to vector<string>
    LogBuffer recent_logs;
    LogSearchIndex search_index;
    unordered_map<DWORD, unique_ptr<LogProcess>> active_processes;
    mutex process_mutex;
    string log_directory = "logs";

    void monitor_process(LogProcess& lp) {
        ofstream log_file(lp.log_file, ios::app);
        if (!log_file) {
            printError("Failed to open log file for " + lp.program_name);
            return;
        }

        while (lp.running) {
            time_t now = time(nullptr);
            string timestamp = formatTime(now);
            string log_entry = timestamp + " | PID: " + to_string(lp.pid) + 
                             " | Program: " + lp.program_name + 
                             " | Activity: Process running" +
                             " | Severity: " + classify_severity("Process running");

            {
                lock_guard<mutex> lock(process_mutex);
                all_logs.push_back(log_entry);
                recent_logs.push(log_entry);
                search_index.insert(log_entry, all_logs.size() - 1);
            }

            log_file << log_entry << "\n";
            log_file.flush();
            this_thread::sleep_for(seconds(1));
        }
    }

    vector<string> load_log_file(const string& filename) {
        vector<string> logs;
        ifstream file(filename);
        string line;

        while (getline(file, line)) {
            if (!line.empty()) {
                logs.push_back(line);
                search_index.insert(line, all_logs.size());
            }
        }
        return logs;
    }

    string classify_severity(const string& log_entry) {
        if (log_entry.find("error") != string::npos ||
            log_entry.find("Error") != string::npos ||
            log_entry.find("ERROR") != string::npos) {
            return "high";
        }
        if (log_entry.find("warn") != string::npos ||
            log_entry.find("WARN") != string::npos) {
            return "medium";
        }
        if (log_entry.find("fail") != string::npos) {
            return "medium";
        }
        return "low";
    }

    vector<string> get_all_log_files() {
        vector<string> files;
        WIN32_FIND_DATAA findFileData;
        HANDLE hFind = FindFirstFileA((log_directory + "\\*.log").c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return files;
        }

        do {
            files.push_back(log_directory + "\\" + findFileData.cFileName);
        } while (FindNextFileA(hFind, &findFileData));

        FindClose(hFind);
        return files;
    }

public:
    LogManager() : search_index(all_logs) {
        CreateDirectoryA(log_directory.c_str(), NULL);
        load_existing_logs();
    }

    ~LogManager() {
        for (auto& pair : active_processes) {
            pair.second->running = false;
            if (pair.second->worker_thread.joinable()) {
                pair.second->worker_thread.join();
            }
        }
    }

    void load_existing_logs() {
        auto log_files = get_all_log_files();
        for (const auto& file : log_files) {
            auto logs = load_log_file(file);
            all_logs.insert(all_logs.end(), logs.begin(), logs.end());
        }
    }

    void start_logging(DWORD pid, const string& program_name) {
        lock_guard<mutex> lock(process_mutex);
        if (active_processes.count(pid)) {
            printWarning("Already logging process " + to_string(pid));
            return;
        }

        string log_file = log_directory + "\\" + program_name + "_" + to_string(pid) + ".log";
        auto lp = make_unique<LogProcess>(pid, program_name, log_file);
        lp->worker_thread = thread(&LogManager::monitor_process, this, ref(*lp));

        active_processes.emplace(pid, move(lp));
        printSuccess("Started logging for PID " + to_string(pid) + " (" + program_name + ")");
    }

    void stop_logging(DWORD pid) {
        unique_ptr<LogProcess> lp;
        {
            lock_guard<mutex> lock(process_mutex);
            auto it = active_processes.find(pid);
            if (it == active_processes.end()) {
                printWarning("No active logging for PID " + to_string(pid));
                return;
            }
            lp = move(it->second);
            active_processes.erase(it);
        }

        lp->running = false;
        if (lp->worker_thread.joinable()) {
            lp->worker_thread.join();
        }
        printSuccess("Stopped logging for PID " + to_string(pid));
    }

    void list_active_logs() {
        lock_guard<mutex> lock(process_mutex);
        if (active_processes.empty()) {
            printWarning("No active log processes");
            return;
        }

        printColored("\nActive Log Processes:\n", 11);
        printColored("----------------------------------------\n", 11);
        printColored(" PID       Program           Log File\n", 11);
        printColored("----------------------------------------\n", 11);

        for (const auto& pair : active_processes) {
            cout << " " << setw(8) << left << pair.first
                << " " << setw(16) << left << pair.second->program_name.substr(0, 15)
                << " " << pair.second->log_file.substr(0, 30) << "...\n";
        }
    }

    void analyze_logs(DWORD pid) {
        string log_file;
        {
            lock_guard<mutex> lock(process_mutex);
            auto it = active_processes.find(pid);
            if (it == active_processes.end()) {
                string pattern = "_" + to_string(pid) + ".log";
                auto all_files = get_all_log_files();
                for (const auto& file : all_files) {
                    if (file.find(pattern) != string::npos) {
                        log_file = file;
                        break;
                    }
                }

                if (log_file.empty()) {
                    printError("No log file found for PID " + to_string(pid));
                    return;
                }
            }
            else {
                log_file = it->second->log_file;
            }
        }

        vector<string> logs = load_log_file(log_file);
        if (logs.empty()) {
            printError("No logs found for PID " + to_string(pid));
            return;
        }

        printHeader("LOG ANALYSIS - PID: " + to_string(pid));
        printColored("Loaded " + to_string(logs.size()) + " log entries\n\n", 10);
        printColored("Available commands:\n", 11);
        printColored("!!errors - Show all error messages\n", 14);
        printColored("!!warnings - Show all warning messages\n", 14);
        printColored("!!stats - Show log statistics\n", 14);
        printColored("!!timeline - Show chronological event timeline\n", 14);
        printColored("!!search <query> - Search log content\n", 14);
        printColored("!!sort <field> - Sort logs by field\n", 14);
        printColored("!!severity <level> - Filter by severity level\n", 14);
        printColored("!!exit - Exit analysis mode\n\n", 12);

        string command;
        while (true) {
            printColored("log-analyzer> ", 11);
            getline(cin, command);

            if (command == "!!exit") break;

            if (command == "!!errors") {
                printColored("\nError messages:\n", 12);
                for (const auto& log : logs) {
                    if (log.find("Severity: high") != string::npos) {
                        cout << log << "\n";
                    }
                }
            }
            else if (command == "!!warnings") {
                printColored("\nWarning messages:\n", 14);
                for (const auto& log : logs) {
                    if (log.find("Severity: medium") != string::npos) {
                        cout << log << "\n";
                    }
                }
            }
            else if (command == "!!stats") {
                unordered_map<string, int> severity_counts;
                for (const auto& log : logs) {
                    size_t pos = log.find("Severity: ");
                    if (pos != string::npos) {
                        string severity = log.substr(pos + 10);
                        severity_counts[severity]++;
                    }
                }

                printColored("\nLog Statistics:\n", 11);
                printColored("Total entries: " + to_string(logs.size()) + "\n\n", 10);

                printColored("By Severity:\n", 11);
                for (const auto& sev_pair : severity_counts) {
                    cout << "  " << setw(15) << left << sev_pair.first << ": " << sev_pair.second << "\n";
                }
            }
            else if (command == "!!timeline") {
                printColored("\nTimeline of Events:\n", 11);
                for (const auto& log : logs) {
                    cout << log << "\n";
                }
            }
            else if (command == "!!search" || command.find("!!search ") == 0) {
                if (command == "!!search") {
                    printError("Please provide a search query. Example: !!search process");
                    continue;
                }
                string query = command.substr(9);
                if (query.empty()) {
                    printError("Search query cannot be empty. Example: !!search process");
                    continue;
                }
                printColored("\nSearch results for '" + query + "':\n", 11);
                bool found = false;
                for (const auto& log : logs) {
                    if (log.find(query) != string::npos) {
                        cout << log << "\n";
                        found = true;
                    }
                }
                if (!found) {
                    printWarning("No matches found for '" + query + "'");
                }
            }
            else if (command.find("!!severity ") == 0) {
                string level = command.substr(11);
                if (level.empty()) {
                    printError("Please specify a severity level (high, medium, or low)");
                    continue;
                }
                printColored("\nLogs with severity '" + level + "':\n", 11);
                bool found = false;
                for (const auto& log : logs) {
                    if (log.find("Severity: " + level) != string::npos) {
                        cout << log << "\n";
                        found = true;
                    }
                }
                if (!found) {
                    printWarning("No logs found with severity level '" + level + "'");
                }
            }
            else {
                printError("Unknown command. Available commands:");
                printColored("!!errors - Show all error messages\n", 14);
                printColored("!!warnings - Show all warning messages\n", 14);
                printColored("!!stats - Show log statistics\n", 14);
                printColored("!!timeline - Show chronological event timeline\n", 14);
                printColored("!!search <query> - Search log content\n", 14);
                printColored("!!severity <level> - Filter by severity level\n", 14);
                printColored("!!exit - Exit analysis mode\n", 14);
            }
        }
    }

    void display_recent_logs() {
        auto recent = recent_logs.to_vector();
        printColored("\nRecent Logs (" + to_string(recent.size()) + "):\n", 11);
        for (const auto& log : recent) {
            cout << log << "\n";
        }
    }
};

// ====================== MAIN MENU ======================

void displayMainMenu() {
    printHeader("WINDOWS LOG MANAGEMENT SYSTEM");
    printMenuOption(1, "Start Log Capture");
    printMenuOption(2, "Stop Log Capture");
    printMenuOption(3, "List Active Logs");
    printMenuOption(4, "Analyze Logs");
    printMenuOption(5, "Show Running Processes");
    printMenuOption(6, "View Recent Logs");
    printMenuOption(7, "Exit");
    printColored("\nEnter your choice: ", 11);
}

int main() {
    LogManager log_manager;
    int choice;
    DWORD pid;
    string input;

    while (true) {
        displayMainMenu();
        cin >> choice;
        cin.ignore();

        switch (choice) {
        case 1: {
            auto processes = getRunningProcesses();
            printHeader("RUNNING PROCESSES");
            printColored(" PID       Process Name\n", 11);
            printColored("------------------------\n", 11);
            for (const auto& process : processes) {
                cout << " " << setw(8) << left << process.first << " " << process.second << "\n";
            }

            printColored("\nEnter PID to monitor: ", 11);
            getline(cin, input);
            
            // Validate PID input
            try {
                pid = stoul(input);
                string program_name = getProcessName(pid);
                log_manager.start_logging(pid, program_name);
            }
            catch (const exception&) {
                printError("Invalid PID. Please enter a valid number.");
            }
            break;
        }
        case 2: {
            printColored("Enter PID to stop monitoring: ", 11);
            getline(cin, input);
            
            // Validate PID input
            try {
                pid = stoul(input);
                log_manager.stop_logging(pid);
            }
            catch (const exception&) {
                printError("Invalid PID. Please enter a valid number.");
            }
            break;
        }
        case 3:
            log_manager.list_active_logs();
            break;
        case 4: {
            printColored("Enter PID to analyze: ", 11);
            getline(cin, input);
            
            // Validate PID input
            try {
                pid = stoul(input);
                log_manager.analyze_logs(pid);
            }
            catch (const exception&) {
                printError("Invalid PID. Please enter a valid number.");
            }
            break;
        }
        case 5: {
            auto processes = getRunningProcesses();
            printHeader("RUNNING PROCESSES");
            printColored(" PID       Process Name\n", 11);
            printColored("------------------------\n", 11);
            for (const auto& process : processes) {
                cout << " " << setw(8) << left << process.first << " " << process.second << "\n";
            }
            break;
        }
        case 6:
            log_manager.display_recent_logs();
            break;
        case 7:
            return 0;
        default:
            printError("Invalid choice. Please try again.");
        }

        printColored("\nPress Enter to continue...", 8);
        cin.ignore();
    }
}
