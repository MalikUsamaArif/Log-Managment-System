# Log-Managment-System

1. Introduction
In modern software environments, especially in systems with multiple processes or services, efficient logging and monitoring are essential. This project presents a C++ Log Management System with advanced features including:
•	Real-time system call monitoring using strace
•	Custom priority queue for log severity management
•	Bloom filter for quick log type checks
•	Graph-based relationship analysis between logs
•	Interactive log analyzer with timeline, statistics, and error tracing

2. Objectives
•	Implement a real-time process log monitoring system.
•	Build custom data structures to manage and analyze logs efficiently.
•	Provide a command-line interface for log analysis and interaction.
•	Enhance performance using multithreading and thread-safe data structures.

3. System Architecture
3.1 Components Overview
•	LogManager: Core controller for starting, stopping, and analyzing logs.
•	PriorityLogQueue: Custom min-heap for priority-based log sorting.
•	LogRelationshipGraph: Graph structure to identify relations between log types.
•	LogTypeFilter: Bloom filter to optimize log type lookups.
•	Monitor Process: Spawns threads to run strace on target PIDs and logs system calls in JSON format.

4. Key Features and Data Structures
4.1 Real-Time Monitoring
Each target process is monitored using strace, and the log output is captured and written to a .log file in JSON format.
4.2 Custom Priority Queue
Used to sort logs based on weight (importance or frequency):
4.3 Log Relationship Graph
Stores weighted connections between different types of logs (e.g., program_name → system_call):
4.4 Bloom Filter
Lightweight mechanism to quickly check if a certain log type has been previously encountered:

5. Interactive Analysis Features
The system supports real-time interaction through commands like:
Command	Functionality
!!errors	Shows all log entries indicating errors
!!stats	Displays count of each log type
!!timeline	Orders logs chronologically
!!search <query>	Searches logs for specific keywords
!!relationships	Prints top N log type relationships
exit	Exits analysis mode

6. Multithreading and Synchronization
•	Threads are used for non-blocking process monitoring.
•	mutex is used to ensure thread safety in all shared data structures.
•	Each LogProcess owns a monitoring thread that tracks a PID independently.
