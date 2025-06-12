# Windows Log Management System

A comprehensive Windows process monitoring and logging system.

## Project Overview

The Windows Log Management System is a C++ application that provides:
- Process monitoring and logging
- Circular buffer for log storage
- Trie-based log search functionality
- Real-time process tracking

## Features

- Real-time process monitoring
- Circular buffer for efficient log storage
- Trie-based search index for fast log retrieval
- Severity-based log classification
- Process activity tracking
- Log file management
- Interactive command-line interface
- Color-coded output for better readability

## Requirements

- Windows OS
- C++ compiler with C++11 support
- Windows SDK

## Installation

1. Clone the repository:
bash
git clone <repository-url>
cd windows-log-management-system


2. Compile the program:
bash
g++ main.cpp -o log_manager


## Usage

### Running the Program
bash
./log_manager


### Main Menu Options
1. Start Log Capture
   - View running processes
   - Select PID to monitor
   - Begin logging process activity

2. Stop Log Capture
   - Stop monitoring selected process
   - Save collected logs

3. List Active Logs
   - View currently monitored processes
   - Display log file locations

4. Analyze Logs
   - View detailed log analysis
   - Search through logs
   - Filter by severity
   - View statistics

5. Show Running Processes
   - List all active processes
   - Display PIDs and process names

6. View Recent Logs
   - Display latest log entries
   - Monitor current activity

7. Exit
   - Safely terminate the program

## Log Analysis Commands

When analyzing logs, the following commands are available:
- !!errors - Show all error messages
- !!warnings - Show all warning messages
- !!stats - Show log statistics
- !!timeline - Show chronological event timeline
- !!search <query> - Search log content
- !!severity <level> - Filter by severity level
- !!exit - Exit analysis mode

## Project Structure


.
└── main.cpp    # Main C++ program


## Contributing

1. Fork the repository
2. Create your feature branch (git checkout -b feature/AmazingFeature)
3. Commit your changes (git commit -m 'Add some AmazingFeature')
4. Push to the branch (git push origin feature/AmazingFeature)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Windows API for process monitoring
- Standard Template Library (STL) for data structures
