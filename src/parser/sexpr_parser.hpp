#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <unordered_map>
#include <functional>

namespace mcp {

class SExprParser : public IExprParser {
public:
    SExprParser();
    ~SExprParser() override = default;

    // IExprParser implementation
    Result<SExpression> Parse(const std::string& expr) override;
    Result<std::string> Serialize(const SExpression& expr) override;
    Result<SExpression> Evaluate(const SExpression& expr) override;
    Result<SExpression> EvaluateInContext(const SExpression& expr) override;
    Result<std::string> FormatDebugOutput(const SExpression& expr) override;

    // Extended functionality
    void RegisterFunction(const std::string& name, 
                         std::function<Result<SExpression>(const std::vector<SExpression>&)> func);
    void RegisterVariable(const std::string& name, const SExpression& value);
    Result<SExpression> EvaluateInContext(const SExpression& expr, 
                                         const std::unordered_map<std::string, SExpression>& context);

    // Utility functions for debugging
    Result<SExpression> ParseMemoryExpression(const std::string& expr, uintptr_t base_address);

private:
    std::unordered_map<std::string, std::function<Result<SExpression>(const std::vector<SExpression>&)>> functions_;
    std::unordered_map<std::string, SExpression> variables_;

    // Parser state
    size_t pos_ = 0;
    std::string input_;
    size_t recursion_depth_ = 0; // SECURITY: Stack overflow protection

    // Parsing methods
    Result<SExpression> ParseExpression();
    Result<SExpression> ParseAtom();
    Result<SExpression> ParseList();
    Result<std::string> ParseString();
    Result<std::string> ParseSymbol();
    Result<int64_t> ParseInteger();
    Result<double> ParseFloat();
    Result<bool> ParseBoolean();

    // Evaluation methods
    Result<SExpression> EvaluateList(const std::vector<SExpression>& list);
    Result<SExpression> ApplyFunction(const std::string& func_name, 
                                     const std::vector<SExpression>& args);
    Result<SExpression> LookupVariable(const std::string& name);

    // Serialization methods
    std::string SerializeAtom(const SExpression& expr);
    std::string SerializeList(const std::vector<SExpression>& list);

    // Utility methods
    void SkipWhitespace();
    char CurrentChar() const;
    char PeekChar(size_t offset = 1) const;
    void Advance();
    bool IsEnd() const;
    bool IsWhitespace(char c) const;
    bool IsSymbolChar(char c) const;
    bool IsDigit(char c) const;

    // Built-in functions
    void RegisterBuiltinFunctions();
    Result<SExpression> BuiltinAdd(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinSubtract(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinMultiply(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinDivide(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinEquals(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinIf(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinList(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinCar(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinCdr(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinCons(const std::vector<SExpression>& args);

    // Memory/debugging specific functions
    Result<SExpression> BuiltinReadMemory(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinFormatHex(const std::vector<SExpression>& args);
    Result<SExpression> BuiltinParsePattern(const std::vector<SExpression>& args);

    // Type checking helpers
    bool IsNumber(const SExpression& expr) const;
    bool IsString(const SExpression& expr) const;
    bool IsList(const SExpression& expr) const;
    bool IsSymbol(const SExpression& expr) const;
    
    double GetNumberValue(const SExpression& expr) const;
    std::string GetStringValue(const SExpression& expr) const;
    std::vector<SExpression> GetListValue(const SExpression& expr) const;
};

} // namespace mcp