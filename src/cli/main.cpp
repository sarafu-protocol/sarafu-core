// Sarafu CLI Tool
// Command-line interface for Sarafu blockchain operations

#include "sarafu/version.h"
#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

// Forward declarations
int cmd_keygen(int argc, char* argv[]);
int cmd_genesis(int argc, char* argv[]);
int cmd_snapshot(int argc, char* argv[]);
int cmd_node(int argc, char* argv[]);
int cmd_version(int argc, char* argv[]);
int cmd_help(int argc, char* argv[]);

// Command registry
struct Command {
    std::string name;
    std::string description;
    std::function<int(int, char**)> handler;
};

static std::vector<Command> commands = {
    {"keygen", "Generate validator keys", cmd_keygen},
    {"genesis", "Generate genesis file", cmd_genesis},
    {"snapshot", "Manage blockchain snapshots", cmd_snapshot},
    {"node", "Run blockchain node", cmd_node},
    {"version", "Show version information", cmd_version},
    {"help", "Show help information", cmd_help}
};

void print_usage() {
    std::cout << "Sarafu Blockchain CLI\n";
    std::cout << "Usage: sar <command> [options]\n\n";
    std::cout << "Commands:\n";
    for (const auto& cmd : commands) {
        std::cout << "  " << cmd.name << "\t\t" << cmd.description << "\n";
    }
    std::cout << "\nRun 'sar <command> --help' for more information on a command.\n";
}

int cmd_help(int argc, char* argv[]) {
    if (argc > 0) {
        // Help for specific command
        std::string cmd_name = argv[0];
        for (const auto& cmd : commands) {
            if (cmd.name == cmd_name) {
                std::cout << "Help for '" << cmd_name << "': " << cmd.description << "\n";
                return 0;
            }
        }
        std::cerr << "Unknown command: " << cmd_name << "\n";
        return 1;
    }
    
    print_usage();
    return 0;
}

int cmd_version(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Sarafu Blockchain v" << sarafu::VERSION << "\n";
    std::cout << "Built with C++17\n";
    return 0;
}

int cmd_node(int argc, char* argv[]) {
    std::cout << "Starting Sarafu node...\n";

    std::vector<char*> args;
    args.reserve(static_cast<size_t>(argc) + 2);
    args.push_back(const_cast<char*>("sarafu-node"));
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    args.push_back(nullptr);

#if defined(_WIN32)
    std::string command = "sarafu-node";
    for (int i = 0; i < argc; ++i) {
        command += " ";
        command += argv[i];
    }
    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Failed to launch sarafu-node (exit code " << result << ")\n";
        return 1;
    }
    return 0;
#else
    execvp("sarafu-node", args.data());
    std::cerr << "Failed to launch sarafu-node: " << std::strerror(errno) << "\n";
    return 1;
#endif
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    
    // Find and execute command
    for (const auto& cmd : commands) {
        if (cmd.name == command) {
            return cmd.handler(argc - 2, argv + 2);
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Run 'sar help' for usage information.\n";
    return 1;
}
