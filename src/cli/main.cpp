// Sarafu CLI Tool
// Command-line interface for Sarafu blockchain operations

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstring>

// Forward declarations
int cmd_keygen(int argc, char* argv[]);
int cmd_genesis(int argc, char* argv[]);
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
    std::cout << "Sarafu Blockchain v0.1.0\n";
    std::cout << "Built with C++17\n";
    return 0;
}

int cmd_node(int argc, char* argv[]) {
    std::cout << "Starting Sarafu node...\n";
    std::cout << "This will launch the full node implementation.\n";
    std::cout << "Use 'sarafu-node' binary directly for now.\n";
    
    // TODO: This could exec() the sarafu-node binary
    // or we could refactor to have node logic in a library
    
    return 0;
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
