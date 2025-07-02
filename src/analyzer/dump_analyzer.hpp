#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace mcp {

class ILogger;

class DumpAnalyzer : public IDumpAnalyzer {
public:
    explicit DumpAnalyzer(std::shared_ptr<ILogger> logger);
    ~DumpAnalyzer() override;

    // IDumpAnalyzer implementation
    Result<std::vector<std::string>> AnalyzePatterns(const MemoryDump& dump) override;
    Result<std::vector<std::string>> FindStrings(const MemoryDump& dump) override;
    Result<std::unordered_map<std::string, std::string>> ExtractMetadata(const MemoryDump& dump) override;

    // Extended functionality
    Result<std::vector<PatternMatch>> FindMalwareSignatures(const MemoryDump& dump);
    Result<std::vector<StringMatch>> ExtractStrings(const MemoryDump& dump, bool include_wide = true);
    Result<AnalysisResult> PerformFullAnalysis(const MemoryDump& dump);
    
    // Pattern management
    void LoadPatternDatabase(const std::string& pattern_file);
    void AddCustomPattern(const std::string& name, const std::vector<uint8_t>& pattern, const std::string& description);
    size_t GetPatternCount() const;

private:
    struct Pattern {
        std::string name;
        std::vector<uint8_t> bytes;
        std::string description;
        double confidence_threshold = 0.8;
    };

    std::shared_ptr<ILogger> logger_;
    std::vector<Pattern> patterns_;
    
    // String extraction
    Result<std::vector<StringMatch>> FindAsciiStrings(const MemoryDump& dump, size_t min_length = 4);
    Result<std::vector<StringMatch>> FindUnicodeStrings(const MemoryDump& dump, size_t min_length = 4);
    
    // Pattern matching
    Result<std::vector<PatternMatch>> SearchPatterns(const MemoryDump& dump);
    double CalculatePatternConfidence(const Pattern& pattern, const MemoryDump& dump, size_t offset);
    
    // Metadata extraction
    Result<std::unordered_map<std::string, std::string>> ExtractPEMetadata(const MemoryDump& dump);
    Result<std::unordered_map<std::string, std::string>> ExtractELFMetadata(const MemoryDump& dump);
    Result<std::unordered_map<std::string, std::string>> ExtractGenericMetadata(const MemoryDump& dump);
    
    // Utility methods
    bool IsValidAsciiChar(uint8_t c) const;
    bool IsValidUnicodeChar(uint16_t c) const;
    std::string BytesToHex(const std::vector<uint8_t>& bytes) const;
    
    // Built-in patterns
    void LoadBuiltinPatterns();
    void AddMalwarePatterns();
    void AddExecutablePatterns();
    void AddCommonPatterns();
};

} // namespace mcp