#include "dump_analyzer.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cmath>

namespace mcp {

DumpAnalyzer::DumpAnalyzer(std::shared_ptr<ILogger> logger) 
    : logger_(logger) {
    
    LoadBuiltinPatterns();
    
    if (logger_) {
        logger_->LogFormatted(ILogger::Level::INFO, 
                            "Dump analyzer initialized with %zu patterns", patterns_.size());
    }
}

DumpAnalyzer::~DumpAnalyzer() {
    if (logger_) {
        logger_->Log(ILogger::Level::INFO, "Dump analyzer destroyed");
    }
}

Result<std::vector<std::string>> DumpAnalyzer::AnalyzePatterns(const MemoryDump& dump) {
    auto pattern_result = SearchPatterns(dump);
    if (!pattern_result.IsSuccess()) {
        return Result<std::vector<std::string>>::Error(pattern_result.Error());
    }
    
    std::vector<std::string> pattern_descriptions;
    for (const auto& match : pattern_result.Value()) {
        std::ostringstream desc;
        desc << match.pattern_name << " at 0x" << std::hex << match.address 
             << " (confidence: " << std::fixed << std::setprecision(2) << match.confidence << ")";
        pattern_descriptions.push_back(desc.str());
    }
    
    return Result<std::vector<std::string>>::Success(pattern_descriptions);
}

Result<std::vector<std::string>> DumpAnalyzer::FindStrings(const MemoryDump& dump) {
    auto string_result = ExtractStrings(dump);
    if (!string_result.IsSuccess()) {
        return Result<std::vector<std::string>>::Error(string_result.Error());
    }
    
    std::vector<std::string> string_values;
    string_values.reserve(string_result.Value().size()); // ОПТИМИЗАЦИЯ: резервируем память
    
    for (const auto& match : string_result.Value()) {
        string_values.emplace_back(match.value); // ОПТИМИЗАЦИЯ: emplace_back вместо push_back
    }
    
    return Result<std::vector<std::string>>::Success(std::move(string_values)); // ОПТИМИЗАЦИЯ: move
}

Result<std::unordered_map<std::string, std::string>> DumpAnalyzer::ExtractMetadata(const MemoryDump& dump) {
    std::unordered_map<std::string, std::string> metadata;
    
    // Basic metadata
    metadata["size"] = std::to_string(dump.size);
    metadata["base_address"] = "0x" + std::to_string(dump.base_address);
    metadata["module"] = dump.module_name;
    
    // Detect file format based on magic bytes
    if (dump.data.size() >= 2) {
        if (dump.data[0] == 'M' && dump.data[1] == 'Z') {
            metadata["format"] = "PE";
            auto pe_metadata = ExtractPEMetadata(dump);
            if (pe_metadata.IsSuccess()) {
                for (const auto& metadata_pair : pe_metadata.Value()) {
                    metadata["pe_" + metadata_pair.first] = metadata_pair.second;
                }
            }
        } else if (dump.data.size() >= 4 && 
                   dump.data[0] == 0x7F && dump.data[1] == 'E' && 
                   dump.data[2] == 'L' && dump.data[3] == 'F') {
            metadata["format"] = "ELF";
            auto elf_metadata = ExtractELFMetadata(dump);
            if (elf_metadata.IsSuccess()) {
                for (const auto& metadata_pair : elf_metadata.Value()) {
                    metadata["elf_" + metadata_pair.first] = metadata_pair.second;
                }
            }
        } else {
            metadata["format"] = "Unknown";
        }
    }
    
    // Add generic analysis results
    auto generic_metadata = ExtractGenericMetadata(dump);
    if (generic_metadata.IsSuccess()) {
        for (const auto& metadata_pair : generic_metadata.Value()) {
            metadata[metadata_pair.first] = metadata_pair.second;
        }
    }
    
    return Result<std::unordered_map<std::string, std::string>>::Success(std::move(metadata)); // ОПТИМИЗАЦИЯ: move
}

Result<std::vector<PatternMatch>> DumpAnalyzer::FindMalwareSignatures(const MemoryDump& dump) {
    std::vector<PatternMatch> malware_matches;
    
    auto all_matches_result = SearchPatterns(dump);
    if (!all_matches_result.IsSuccess()) {
        return Result<std::vector<PatternMatch>>::Error(all_matches_result.Error());
    }
    
    // Filter for malware-related patterns
    for (const auto& match : all_matches_result.Value()) {
        if (match.pattern_name.find("malware") != std::string::npos ||
            match.pattern_name.find("virus") != std::string::npos ||
            match.pattern_name.find("trojan") != std::string::npos) {
            malware_matches.push_back(match);
        }
    }
    
    return Result<std::vector<PatternMatch>>::Success(malware_matches);
}

Result<std::vector<StringMatch>> DumpAnalyzer::ExtractStrings(const MemoryDump& dump, bool include_wide) {
    std::vector<StringMatch> all_strings;
    
    // Extract ASCII strings
    auto ascii_result = FindAsciiStrings(dump);
    if (ascii_result.IsSuccess()) {
        for (auto& str_match : ascii_result.Value()) {
            all_strings.push_back(str_match);
        }
    }
    
    // Extract Unicode strings if requested
    if (include_wide) {
        auto unicode_result = FindUnicodeStrings(dump);
        if (unicode_result.IsSuccess()) {
            for (auto& str_match : unicode_result.Value()) {
                all_strings.push_back(str_match);
            }
        }
    }
    
    // Sort by address
    std::sort(all_strings.begin(), all_strings.end(), 
              [](const StringMatch& a, const StringMatch& b) {
                  return a.address < b.address;
              });
    
    return Result<std::vector<StringMatch>>::Success(all_strings);
}

Result<AnalysisResult> DumpAnalyzer::PerformFullAnalysis(const MemoryDump& dump) {
    AnalysisResult result;
    result.timestamp = std::chrono::system_clock::now();
    
    // Pattern analysis
    auto pattern_result = SearchPatterns(dump);
    if (pattern_result.IsSuccess()) {
        result.patterns = pattern_result.Value();
    }
    
    // String extraction
    auto string_result = ExtractStrings(dump);
    if (string_result.IsSuccess()) {
        result.strings = string_result.Value();
    }
    
    // Metadata extraction
    auto metadata_result = ExtractMetadata(dump);
    if (metadata_result.IsSuccess()) {
        result.metadata = metadata_result.Value();
    }
    
    if (logger_) {
        logger_->LogFormatted(ILogger::Level::INFO, 
                            "Full analysis completed: %zu patterns, %zu strings", 
                            result.patterns.size(), result.strings.size());
    }
    
    return Result<AnalysisResult>::Success(result);
}

void DumpAnalyzer::AddCustomPattern(const std::string& name, const std::vector<uint8_t>& pattern, 
                                   const std::string& description) {
    Pattern custom_pattern;
    custom_pattern.name = name;
    custom_pattern.bytes = pattern;
    custom_pattern.description = description;
    custom_pattern.confidence_threshold = 0.9; // Higher threshold for custom patterns
    
    patterns_.push_back(custom_pattern);
    
    if (logger_) {
        logger_->LogFormatted(ILogger::Level::DEBUG, "Added custom pattern: %s", name.c_str());
    }
}

size_t DumpAnalyzer::GetPatternCount() const {
    return patterns_.size();
}

Result<std::vector<StringMatch>> DumpAnalyzer::FindAsciiStrings(const MemoryDump& dump, size_t min_length) {
    std::vector<StringMatch> strings;
    
    std::string current_string;
    uintptr_t string_start = 0;
    
    for (size_t i = 0; i < dump.data.size(); ++i) {
        uint8_t byte = dump.data[i];
        
        if (IsValidAsciiChar(byte)) {
            if (current_string.empty()) {
                string_start = dump.base_address + i;
            }
            current_string += static_cast<char>(byte);
        } else {
            if (current_string.length() >= min_length) {
                StringMatch match;
                match.address = string_start;
                match.value = current_string;
                match.encoding = "ASCII";
                match.length = current_string.length();
                match.is_wide = false;
                
                strings.push_back(match);
            }
            current_string.clear();
        }
    }
    
    // Handle string at end of data
    if (current_string.length() >= min_length) {
        StringMatch match;
        match.address = string_start;
        match.value = current_string;
        match.encoding = "ASCII";
        match.length = current_string.length();
        match.is_wide = false;
        
        strings.push_back(match);
    }
    
    return Result<std::vector<StringMatch>>::Success(strings);
}

Result<std::vector<StringMatch>> DumpAnalyzer::FindUnicodeStrings(const MemoryDump& dump, size_t min_length) {
    std::vector<StringMatch> strings;
    
    std::string current_string;
    uintptr_t string_start = 0;
    
    for (size_t i = 0; i < dump.data.size() - 1; i += 2) {
        uint16_t wide_char = *reinterpret_cast<const uint16_t*>(&dump.data[i]);
        
        if (IsValidUnicodeChar(wide_char) && wide_char < 256) {
            if (current_string.empty()) {
                string_start = dump.base_address + i;
            }
            current_string += static_cast<char>(wide_char & 0xFF);
        } else {
            if (current_string.length() >= min_length) {
                StringMatch match;
                match.address = string_start;
                match.value = current_string;
                match.encoding = "Unicode";
                match.length = current_string.length() * 2; // Byte length
                match.is_wide = true;
                
                strings.push_back(match);
            }
            current_string.clear();
        }
    }
    
    return Result<std::vector<StringMatch>>::Success(strings);
}

Result<std::vector<PatternMatch>> DumpAnalyzer::SearchPatterns(const MemoryDump& dump) {
    std::vector<PatternMatch> matches;
    
    for (const auto& pattern : patterns_) {
        // Simple pattern search
        for (size_t i = 0; i <= dump.data.size() - pattern.bytes.size(); ++i) {
            bool match_found = true;
            
            for (size_t j = 0; j < pattern.bytes.size(); ++j) {
                if (dump.data[i + j] != pattern.bytes[j]) {
                    match_found = false;
                    break;
                }
            }
            
            if (match_found) {
                double confidence = CalculatePatternConfidence(pattern, dump, i);
                
                if (confidence >= pattern.confidence_threshold) {
                    PatternMatch match;
                    match.address = dump.base_address + i;
                    match.size = pattern.bytes.size();
                    match.pattern_name = pattern.name;
                    match.description = pattern.description;
                    match.confidence = confidence;
                    
                    matches.push_back(match);
                }
                
                // Skip ahead to avoid overlapping matches
                i += pattern.bytes.size() - 1;
            }
        }
    }
    
    return Result<std::vector<PatternMatch>>::Success(matches);
}

double DumpAnalyzer::CalculatePatternConfidence(const Pattern& pattern, const MemoryDump& /* dump */, size_t /* offset */) {
    // Simple confidence calculation based on pattern rarity
    double base_confidence = 0.8;
    
    // Longer patterns get higher confidence
    if (pattern.bytes.size() > 8) {
        base_confidence += 0.1;
    }
    
    // Patterns with common bytes get lower confidence
    size_t common_bytes = 0;
    for (uint8_t byte : pattern.bytes) {
        if (byte == 0x00 || byte == 0xFF || byte == 0x90) { // NOP, null, etc.
            common_bytes++;
        }
    }
    
    if (common_bytes > pattern.bytes.size() / 2) {
        base_confidence -= 0.2;
    }
    
    return std::max(0.0, std::min(1.0, base_confidence));
}

Result<std::unordered_map<std::string, std::string>> DumpAnalyzer::ExtractPEMetadata(const MemoryDump& dump) {
    std::unordered_map<std::string, std::string> metadata;
    
    if (dump.data.size() < 64) {
        return Result<std::unordered_map<std::string, std::string>>::Error("Data too small for PE analysis");
    }
    
    // Basic PE header analysis (stub implementation)
    metadata["signature"] = "MZ";
    metadata["type"] = "PE";
    
    return Result<std::unordered_map<std::string, std::string>>::Success(std::move(metadata)); // ОПТИМИЗАЦИЯ: move
}

Result<std::unordered_map<std::string, std::string>> DumpAnalyzer::ExtractELFMetadata(const MemoryDump& dump) {
    std::unordered_map<std::string, std::string> metadata;
    
    if (dump.data.size() < 16) {
        return Result<std::unordered_map<std::string, std::string>>::Error("Data too small for ELF analysis");
    }
    
    // Basic ELF header analysis (stub implementation)
    metadata["signature"] = "ELF";
    metadata["type"] = "ELF";
    
    return Result<std::unordered_map<std::string, std::string>>::Success(std::move(metadata)); // ОПТИМИЗАЦИЯ: move
}

Result<std::unordered_map<std::string, std::string>> DumpAnalyzer::ExtractGenericMetadata(const MemoryDump& dump) {
    std::unordered_map<std::string, std::string> metadata;
    
    // Calculate entropy
    std::vector<size_t> byte_counts(256, 0);
    for (uint8_t byte : dump.data) {
        byte_counts[byte]++;
    }
    
    double entropy = 0.0;
    for (size_t count : byte_counts) {
        if (count > 0) {
            double probability = static_cast<double>(count) / dump.data.size();
            entropy -= probability * (std::log(probability) / std::log(2.0));
        }
    }
    
    metadata["entropy"] = std::to_string(entropy);
    
    // Count null bytes
    size_t null_bytes = byte_counts[0];
    metadata["null_byte_percentage"] = std::to_string((null_bytes * 100.0) / dump.data.size());
    
    return Result<std::unordered_map<std::string, std::string>>::Success(std::move(metadata)); // ОПТИМИЗАЦИЯ: move
}

bool DumpAnalyzer::IsValidAsciiChar(uint8_t c) const {
    return (c >= 32 && c <= 126) || c == '\t' || c == '\n' || c == '\r';
}

bool DumpAnalyzer::IsValidUnicodeChar(uint16_t c) const {
    return c > 0 && c < 256 && IsValidAsciiChar(static_cast<uint8_t>(c));
}

std::string DumpAnalyzer::BytesToHex(const std::vector<uint8_t>& bytes) const {
    std::ostringstream hex_stream;
    hex_stream << std::hex << std::setfill('0');
    
    for (uint8_t byte : bytes) {
        hex_stream << std::setw(2) << static_cast<int>(byte);
    }
    
    return hex_stream.str();
}

void DumpAnalyzer::LoadBuiltinPatterns() {
    AddMalwarePatterns();
    AddExecutablePatterns();
    AddCommonPatterns();
}

void DumpAnalyzer::AddMalwarePatterns() {
    // Common malware signatures (simplified examples)
    AddCustomPattern("malware_CreateRemoteThread", 
                    {0xFF, 0x15, 0x00, 0x00, 0x00, 0x00}, // CALL CreateRemoteThread
                    "Potential process injection");
    
    AddCustomPattern("malware_WriteProcessMemory",
                    {0x6A, 0x04, 0x68, 0x00, 0x10, 0x00, 0x00}, // Common WPM pattern
                    "Potential memory modification");
}

void DumpAnalyzer::AddExecutablePatterns() {
    // Common executable patterns
    AddCustomPattern("pe_mz_header", 
                    {'M', 'Z'}, 
                    "PE executable header");
    
    AddCustomPattern("elf_header", 
                    {0x7F, 'E', 'L', 'F'}, 
                    "ELF executable header");
}

void DumpAnalyzer::AddCommonPatterns() {
    // Common instruction patterns
    AddCustomPattern("nop_sled", 
                    {0x90, 0x90, 0x90, 0x90}, 
                    "NOP sled (potential shellcode)");
    
    AddCustomPattern("call_pop", 
                    {0xE8, 0x00, 0x00, 0x00, 0x00, 0x58}, 
                    "CALL/POP technique");
}

} // namespace mcp