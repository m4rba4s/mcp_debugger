#include "sexpr_parser.hpp"
#include <cctype>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>

namespace mcp {

SExprParser::SExprParser() {
    RegisterBuiltinFunctions();
}

Result<SExpression> SExprParser::Parse(const std::string& expr) {
    // ЗАЩИТА ОТ DOS: ограничиваем размер входных данных
    constexpr size_t MAX_EXPRESSION_SIZE = 1024 * 1024; // 1MB
    if (expr.length() > MAX_EXPRESSION_SIZE) {
        return Result<SExpression>::Error("Expression too large (max 1MB)");
    }
    
    input_ = expr;
    pos_ = 0;
    recursion_depth_ = 0; // Сбрасываем счетчик рекурсии
    
    SkipWhitespace();
    if (IsEnd()) {
        return Result<SExpression>::Error("Empty expression");
    }
    
    return ParseExpression();
}

Result<std::string> SExprParser::Serialize(const SExpression& expr) {
    if (expr.IsAtom()) {
        return Result<std::string>::Success(SerializeAtom(expr));
    } else {
        std::vector<SExpression> list = std::get<std::vector<SExpression>>(expr.value);
        return Result<std::string>::Success(SerializeList(list));
    }
}

Result<SExpression> SExprParser::Evaluate(const SExpression& expr) {
    if (expr.IsAtom()) {
        // Check if it's a variable reference
        if (std::holds_alternative<std::string>(expr.value)) {
            std::string symbol = std::get<std::string>(expr.value);
            auto var_result = LookupVariable(symbol);
            if (var_result.IsSuccess()) {
                return var_result;
            }
        }
        // Return atom as-is
        return Result<SExpression>::Success(expr);
    }
    
    // Evaluate list
    std::vector<SExpression> list = std::get<std::vector<SExpression>>(expr.value);
    return EvaluateList(list);
}

void SExprParser::RegisterFunction(const std::string& name, 
                                  std::function<Result<SExpression>(const std::vector<SExpression>&)> func) {
    functions_[name] = func;
}

void SExprParser::RegisterVariable(const std::string& name, const SExpression& value) {
    variables_[name] = value;
}

Result<SExpression> SExprParser::EvaluateInContext(const SExpression& expr, 
                                                  const std::unordered_map<std::string, SExpression>& context) {
    // Temporarily merge context with current variables
    auto old_variables = variables_;
    for (const auto& [name, value] : context) {
        variables_[name] = value;
    }
    
    auto result = Evaluate(expr);
    
    // Restore original variables
    variables_ = old_variables;
    
    return result;
}

Result<SExpression> SExprParser::ParseMemoryExpression(const std::string& expr, uintptr_t base_address) {
    // Register memory base address as variable
    SExpression base_expr;
    base_expr.value = static_cast<int64_t>(base_address);
    RegisterVariable("base-addr", base_expr);
    
    return Parse(expr);
}

Result<std::string> SExprParser::FormatDebugOutput(const SExpression& expr) {
    std::ostringstream oss;
    
    if (expr.IsAtom()) {
        if (std::holds_alternative<int64_t>(expr.value)) {
            int64_t val = std::get<int64_t>(expr.value);
            oss << "0x" << std::hex << val << " (" << std::dec << val << ")";
        } else if (std::holds_alternative<double>(expr.value)) {
            oss << std::get<double>(expr.value);
        } else if (std::holds_alternative<std::string>(expr.value)) {
            oss << "\"" << std::get<std::string>(expr.value) << "\"";
        } else if (std::holds_alternative<bool>(expr.value)) {
            oss << (std::get<bool>(expr.value) ? "true" : "false");
        }
    } else {
        oss << "(list with " << std::get<std::vector<SExpression>>(expr.value).size() << " elements)";
    }
    
    return Result<std::string>::Success(oss.str());
}

Result<SExpression> SExprParser::ParseExpression() {
    // ЗАЩИТА ОТ STACK OVERFLOW: ограничиваем глубину рекурсии
    constexpr size_t MAX_RECURSION_DEPTH = 100;
    if (++recursion_depth_ > MAX_RECURSION_DEPTH) {
        return Result<SExpression>::Error("Maximum recursion depth exceeded (100 levels)");
    }
    
    // RAII guard для автоматического уменьшения счетчика
    struct RecursionGuard {
        size_t& depth;
        RecursionGuard(size_t& d) : depth(d) {}
        ~RecursionGuard() { --depth; }
    } guard(recursion_depth_);
    
    SkipWhitespace();
    
    if (IsEnd()) {
        return Result<SExpression>::Error("Unexpected end of input");
    }
    
    char c = CurrentChar();
    if (c == '(') {
        return ParseList();
    } else {
        return ParseAtom();
    }
}

Result<SExpression> SExprParser::ParseAtom() {
    SkipWhitespace();
    
    char c = CurrentChar();
    
    if (c == '"') {
        auto str_result = ParseString();
        if (!str_result.IsSuccess()) {
            return Result<SExpression>::Error(str_result.Error());
        }
        
        SExpression expr;
        expr.value = str_result.Value();
        return Result<SExpression>::Success(expr);
    }
    
    if (IsDigit(c) || c == '-' || c == '+') {
        // Try to parse as number
        size_t start_pos = pos_;
        std::string num_str;
        
        if (c == '-' || c == '+') {
            num_str += c;
            Advance();
        }
        
        bool has_dot = false;
        while (!IsEnd() && (IsDigit(CurrentChar()) || CurrentChar() == '.')) {
            if (CurrentChar() == '.') {
                if (has_dot) break; // Second dot, invalid
                has_dot = true;
            }
            num_str += CurrentChar();
            Advance();
        }
        
        if (num_str.empty() || num_str == "-" || num_str == "+") {
            pos_ = start_pos;
            return ParseSymbol().IsSuccess() ? 
                Result<SExpression>::Success(SExpression{{ParseSymbol().Value()}}) :
                Result<SExpression>::Error("Invalid symbol");
        }
        
        SExpression expr;
        if (has_dot) {
            try {
                expr.value = std::stod(num_str);
            } catch (...) {
                return Result<SExpression>::Error("Invalid float: " + num_str);
            }
        } else {
            try {
                // БЕЗОПАСНОСТЬ: проверяем длину перед конверсией
                if (num_str.length() > 18) { // max digits for int64_t
                    return Result<SExpression>::Error("Integer too large: " + num_str);
                }
                
                long long value = std::stoll(num_str);
                
                // Дополнительная проверка диапазона int64_t
                if (value < std::numeric_limits<int64_t>::min() || 
                    value > std::numeric_limits<int64_t>::max()) {
                    return Result<SExpression>::Error("Integer out of range: " + num_str);
                }
                
                expr.value = static_cast<int64_t>(value);
            } catch (const std::out_of_range&) {
                return Result<SExpression>::Error("Integer out of range: " + num_str);
            } catch (const std::invalid_argument&) {
                return Result<SExpression>::Error("Invalid integer format: " + num_str);
            } catch (...) {
                return Result<SExpression>::Error("Invalid integer: " + num_str);
            }
        }
        
        return Result<SExpression>::Success(expr);
    }
    
    // Parse as symbol
    auto symbol_result = ParseSymbol();
    if (!symbol_result.IsSuccess()) {
        return Result<SExpression>::Error(symbol_result.Error());
    }
    
    std::string symbol = symbol_result.Value();
    
    // Check for boolean literals
    if (symbol == "true" || symbol == "#t") {
        SExpression expr;
        expr.value = true;
        return Result<SExpression>::Success(expr);
    }
    
    if (symbol == "false" || symbol == "#f") {
        SExpression expr;
        expr.value = false;
        return Result<SExpression>::Success(expr);
    }
    
    // Return as symbol
    SExpression expr;
    expr.value = symbol;
    return Result<SExpression>::Success(expr);
}

Result<SExpression> SExprParser::ParseList() {
    if (CurrentChar() != '(') {
        return Result<SExpression>::Error("Expected '('");
    }
    
    Advance(); // Skip '('
    SkipWhitespace();
    
    std::vector<SExpression> elements;
    // ЗАЩИТА ОТ DOS: ограничиваем количество элементов в списке
    constexpr size_t MAX_LIST_ELEMENTS = 10000;
    
    while (!IsEnd() && CurrentChar() != ')') {
        if (elements.size() >= MAX_LIST_ELEMENTS) {
            return Result<SExpression>::Error("List too large (max 10000 elements)");
        }
        
        auto element_result = ParseExpression();
        if (!element_result.IsSuccess()) {
            return element_result;
        }
        
        elements.push_back(std::move(element_result.Value())); // Используем move для оптимизации
        SkipWhitespace();
    }
    
    if (IsEnd()) {
        return Result<SExpression>::Error("Missing closing ')'");
    }
    
    Advance(); // Skip ')'
    
    SExpression expr;
    expr.value = std::move(elements); // Move optimization
    return Result<SExpression>::Success(std::move(expr));
}

Result<std::string> SExprParser::ParseString() {
    if (CurrentChar() != '"') {
        return Result<std::string>::Error("Expected '\"'");
    }
    
    Advance(); // Skip opening quote
    
    std::string result;
    // ЗАЩИТА ОТ DOS: ограничиваем максимальную длину строки
    constexpr size_t MAX_STRING_LENGTH = 64 * 1024; // 64KB
    result.reserve(256); // Резервируем разумное количество памяти
    
    while (!IsEnd() && CurrentChar() != '"') {
        if (result.length() >= MAX_STRING_LENGTH) {
            return Result<std::string>::Error("String too long (max 64KB)");
        }
        
        char c = CurrentChar();
        
        // БЕЗОПАСНОСТЬ: фильтруем опасные символы
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            return Result<std::string>::Error("Invalid control character in string");
        }
        
        if (c == '\\') {
            Advance();
            if (IsEnd()) {
                return Result<std::string>::Error("Unterminated string escape");
            }
            
            char escaped = CurrentChar();
            switch (escaped) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case '0': result += '\0'; break; // Null character escape
                default: 
                    // БЕЗОПАСНОСТЬ: строгая проверка escape последовательностей
                    if (escaped >= 32 && escaped <= 126) {
                        result += '\\';
                        result += escaped;
                    } else {
                        return Result<std::string>::Error("Invalid escape sequence");
                    }
                    break;
            }
        } else {
            result += c;
        }
        Advance();
    }
    
    if (IsEnd()) {
        return Result<std::string>::Error("Unterminated string");
    }
    
    Advance(); // Skip closing quote
    return Result<std::string>::Success(std::move(result)); // Move optimization
}

Result<std::string> SExprParser::ParseSymbol() {
    std::string result;
    
    while (!IsEnd() && IsSymbolChar(CurrentChar())) {
        result += CurrentChar();
        Advance();
    }
    
    if (result.empty()) {
        return Result<std::string>::Error("Empty symbol");
    }
    
    return Result<std::string>::Success(result);
}

Result<SExpression> SExprParser::EvaluateList(const std::vector<SExpression>& list) {
    if (list.empty()) {
        SExpression expr;
        expr.value = std::vector<SExpression>{};
        return Result<SExpression>::Success(expr);
    }
    
    // First element should be a function name
    auto func_result = Evaluate(list[0]);
    if (!func_result.IsSuccess()) {
        return func_result;
    }
    
    if (!std::holds_alternative<std::string>(func_result.Value().value)) {
        return Result<SExpression>::Error("First element of list must be a function name");
    }
    
    std::string func_name = std::get<std::string>(func_result.Value().value);
    
    // Evaluate arguments
    std::vector<SExpression> args;
    for (size_t i = 1; i < list.size(); ++i) {
        auto arg_result = Evaluate(list[i]);
        if (!arg_result.IsSuccess()) {
            return arg_result;
        }
        args.push_back(arg_result.Value());
    }
    
    return ApplyFunction(func_name, args);
}

Result<SExpression> SExprParser::ApplyFunction(const std::string& func_name, 
                                              const std::vector<SExpression>& args) {
    auto it = functions_.find(func_name);
    if (it == functions_.end()) {
        return Result<SExpression>::Error("Unknown function: " + func_name);
    }
    
    return it->second(args);
}

Result<SExpression> SExprParser::LookupVariable(const std::string& name) {
    auto it = variables_.find(name);
    if (it == variables_.end()) {
        return Result<SExpression>::Error("Unknown variable: " + name);
    }
    
    return Result<SExpression>::Success(it->second);
}

std::string SExprParser::SerializeAtom(const SExpression& expr) {
    std::ostringstream oss;
    
    std::visit([&oss](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
            oss << "\"" << value << "\""; // TODO: Proper string escaping
        } else if constexpr (std::is_same_v<T, int64_t>) {
            oss << value;
        } else if constexpr (std::is_same_v<T, double>) {
            oss << value;
        } else if constexpr (std::is_same_v<T, bool>) {
            oss << (value ? "true" : "false");
        }
    }, expr.value);
    
    return oss.str();
}

std::string SExprParser::SerializeList(const std::vector<SExpression>& list) {
    std::ostringstream oss;
    oss << "(";
    
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) oss << " ";
        
        if (list[i].IsAtom()) {
            oss << SerializeAtom(list[i]);
        } else {
            oss << SerializeList(std::get<std::vector<SExpression>>(list[i].value));
        }
    }
    
    oss << ")";
    return oss.str();
}

void SExprParser::SkipWhitespace() {
    while (!IsEnd() && IsWhitespace(CurrentChar())) {
        Advance();
    }
}

char SExprParser::CurrentChar() const {
    return IsEnd() ? '\0' : input_[pos_];
}

char SExprParser::PeekChar(size_t offset) const {
    size_t peek_pos = pos_ + offset;
    return peek_pos >= input_.length() ? '\0' : input_[peek_pos];
}

void SExprParser::Advance() {
    if (!IsEnd()) {
        pos_++;
    }
}

bool SExprParser::IsEnd() const {
    return pos_ >= input_.length();
}

bool SExprParser::IsWhitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool SExprParser::IsSymbolChar(char c) const {
    return std::isalnum(c) || c == '-' || c == '_' || c == '+' || c == '*' || 
           c == '/' || c == '=' || c == '<' || c == '>' || c == '?' || c == '!';
}

bool SExprParser::IsDigit(char c) const {
    return std::isdigit(c);
}

void SExprParser::RegisterBuiltinFunctions() {
    functions_["+"] = [this](const std::vector<SExpression>& args) { return BuiltinAdd(args); };
    functions_["-"] = [this](const std::vector<SExpression>& args) { return BuiltinSubtract(args); };
    functions_["*"] = [this](const std::vector<SExpression>& args) { return BuiltinMultiply(args); };
    functions_["/"] = [this](const std::vector<SExpression>& args) { return BuiltinDivide(args); };
    functions_["="] = [this](const std::vector<SExpression>& args) { return BuiltinEquals(args); };
    functions_["if"] = [this](const std::vector<SExpression>& args) { return BuiltinIf(args); };
    functions_["list"] = [this](const std::vector<SExpression>& args) { return BuiltinList(args); };
    functions_["car"] = [this](const std::vector<SExpression>& args) { return BuiltinCar(args); };
    functions_["cdr"] = [this](const std::vector<SExpression>& args) { return BuiltinCdr(args); };
    functions_["cons"] = [this](const std::vector<SExpression>& args) { return BuiltinCons(args); };
    
    // Debugging specific functions
    functions_["read-memory"] = [this](const std::vector<SExpression>& args) { return BuiltinReadMemory(args); };
    functions_["format-hex"] = [this](const std::vector<SExpression>& args) { return BuiltinFormatHex(args); };
    functions_["parse-pattern"] = [this](const std::vector<SExpression>& args) { return BuiltinParsePattern(args); };
}

Result<SExpression> SExprParser::BuiltinAdd(const std::vector<SExpression>& args) {
    if (args.empty()) {
        SExpression result;
        result.value = static_cast<int64_t>(0);
        return Result<SExpression>::Success(result);
    }
    
    double sum = 0.0;
    bool is_float = false;
    
    for (const auto& arg : args) {
        if (!IsNumber(arg)) {
            return Result<SExpression>::Error("+ requires numeric arguments");
        }
        
        double val = GetNumberValue(arg);
        sum += val;
        
        if (std::holds_alternative<double>(arg.value)) {
            is_float = true;
        }
    }
    
    SExpression result;
    if (is_float) {
        result.value = sum;
    } else {
        result.value = static_cast<int64_t>(sum);
    }
    
    return Result<SExpression>::Success(result);
}

// Additional builtin functions implementation would continue here...
// For brevity, showing structure for one function

bool SExprParser::IsNumber(const SExpression& expr) const {
    return std::holds_alternative<int64_t>(expr.value) || 
           std::holds_alternative<double>(expr.value);
}

double SExprParser::GetNumberValue(const SExpression& expr) const {
    if (std::holds_alternative<int64_t>(expr.value)) {
        return static_cast<double>(std::get<int64_t>(expr.value));
    } else if (std::holds_alternative<double>(expr.value)) {
        return std::get<double>(expr.value);
    }
    return 0.0;
}

// Stub implementations for remaining built-in functions
Result<SExpression> SExprParser::BuiltinSubtract(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Subtract not implemented");
}

Result<SExpression> SExprParser::BuiltinMultiply(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Multiply not implemented");
}

Result<SExpression> SExprParser::BuiltinDivide(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Divide not implemented");
}

Result<SExpression> SExprParser::BuiltinEquals(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Equals not implemented");
}

Result<SExpression> SExprParser::BuiltinIf(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("If not implemented");
}

Result<SExpression> SExprParser::BuiltinList(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("List not implemented");
}

Result<SExpression> SExprParser::BuiltinCar(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Car not implemented");
}

Result<SExpression> SExprParser::BuiltinCdr(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Cdr not implemented");
}

Result<SExpression> SExprParser::BuiltinCons(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("Cons not implemented");
}

Result<SExpression> SExprParser::BuiltinReadMemory(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("ReadMemory not implemented");
}

Result<SExpression> SExprParser::BuiltinFormatHex(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("FormatHex not implemented");
}

Result<SExpression> SExprParser::BuiltinParsePattern(const std::vector<SExpression>& /* args */) {
    return Result<SExpression>::Error("ParsePattern not implemented");
}

Result<SExpression> SExprParser::EvaluateInContext(const SExpression& expr) {
    // Enhanced evaluation with context support
    // For now, delegate to standard evaluation
    return Evaluate(expr);
}


} // namespace mcp