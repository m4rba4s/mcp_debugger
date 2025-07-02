#include "cli_interface.hpp"
#include "mcp/interfaces.hpp"
#include <memory>
#include <iostream>

// Forward declaration for core engine
namespace mcp {
    class CoreEngine;
    std::shared_ptr<ICoreEngine> CreateCoreEngine();
}

int main(int argc, const char* argv[]) {
    try {
        // Create core engine
        auto core_engine = mcp::CreateCoreEngine();
        if (!core_engine) {
            std::cerr << "Failed to create core engine" << std::endl;
            return 1;
        }
        
        // Create CLI interface
        mcp::CLIInterface cli(core_engine);
        
        // Run the application
        return cli.Run(argc, argv);
        
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}