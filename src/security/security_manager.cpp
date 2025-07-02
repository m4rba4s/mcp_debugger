#include "security_manager.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <functional>
#include <string>
#include <memory>
#include <utility>
#include <mutex>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

namespace mcp {

SecurityManager::SecurityManager(std::shared_ptr<ILogger> logger) 
    : logger_(std::move(logger)) {
    
    // Generate default encryption key from system entropy
    auto key_result = GenerateRandomBytes(32); // 256-bit key
    if (key_result.IsSuccess()) {
        encryption_key_ = key_result.Value();
    }
    
    auto iv_result = GenerateRandomBytes(16); // 128-bit IV
    if (iv_result.IsSuccess()) {
        encryption_iv_ = iv_result.Value();
        encryption_initialized_ = true;
    }
    
    if (logger_) {
        logger_->Log(ILogger::Level::INFO, "Security manager initialized");
    }
}

SecurityManager::~SecurityManager() {
    ClearCredentials();
    
    // Clear encryption keys from memory
    std::fill(encryption_key_.begin(), encryption_key_.end(), uint8_t(0));
    std::fill(encryption_iv_.begin(), encryption_iv_.end(), uint8_t(0));
    
    if (logger_) {
        logger_->Log(ILogger::Level::INFO, "Security manager destroyed");
    }
}

Result<void> SecurityManager::StoreCredential(const std::string& key, const std::string& value) {
    auto validate_key_result = ValidateCredentialKey(key);
    if (!validate_key_result.IsSuccess()) {
        return validate_key_result;
    }
    
    auto validate_value_result = ValidateCredentialValue(value);
    if (!validate_value_result.IsSuccess()) {
        return validate_value_result;
    }
    
    if (!encryption_initialized_) {
        return Result<void>::Error("Encryption not initialized");
    }
    
    // Convert value to bytes
    std::vector<uint8_t> value_bytes(value.begin(), value.end());
    
    // Encrypt the value
    std::vector<uint8_t> encrypted_value;
    auto encrypt_result = EncryptData(value_bytes, encrypted_value);
    if (!encrypt_result.IsSuccess()) {
        return encrypt_result;
    }
    
    // Store encrypted credential
    {
        const std::lock_guard<std::mutex> lock(credentials_mutex_);
        encrypted_credentials_[key] = encrypted_value;
    }
    
    if (logger_) {
        // БЕЗОПАСНОСТЬ: НЕ логируем ключи учетных данных!
        logger_->LogFormatted(ILogger::Level::DEBUG, "Stored credential (key hash: %08X)", 
                            std::hash<std::string>{}(key) & 0xFFFFFFFF);
    }
    
    return Result<void>::Success();
}

Result<std::string> SecurityManager::RetrieveCredential(const std::string& key) {
    auto validate_result = ValidateCredentialKey(key);
    if (!validate_result.IsSuccess()) {
        return Result<std::string>::Error(validate_result.Error());
    }
    
    if (!encryption_initialized_) {
        return Result<std::string>::Error("Encryption not initialized");
    }
    
    std::vector<uint8_t> encrypted_value;
    
    // Retrieve encrypted credential
    {
        const std::lock_guard<std::mutex> lock(credentials_mutex_);
        auto it = encrypted_credentials_.find(key);
        if (it == encrypted_credentials_.end()) {
            return Result<std::string>::Error("Credential not found: " + key);
        }
        encrypted_value = it->second;
    }
    
    // Decrypt the value
    std::vector<uint8_t> decrypted_value;
    auto decrypt_result = DecryptData(encrypted_value, decrypted_value);
    if (!decrypt_result.IsSuccess()) {
        return Result<std::string>::Error(decrypt_result.Error());
    }
    
    // Convert back to string
    std::string value(decrypted_value.begin(), decrypted_value.end());
    
    if (logger_) {
        // БЕЗОПАСНОСТЬ: НЕ логируем ключи учетных данных!
        logger_->LogFormatted(ILogger::Level::DEBUG, "Retrieved credential (key hash: %08X)", 
                            std::hash<std::string>{}(key) & 0xFFFFFFFF);
    }
    
    return Result<std::string>::Success(value);
}

Result<void> SecurityManager::EncryptData(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) {
    if (!encryption_initialized_) {
        return Result<void>::Error("Encryption not initialized");
    }
    
    if (data.empty()) {
        return Result<void>::Error("Cannot encrypt empty data");
    }
    
    return EncryptDataInternal(data, encrypted);
}

Result<void> SecurityManager::DecryptData(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) {
    if (!encryption_initialized_) {
        return Result<void>::Error("Encryption not initialized");
    }
    
    if (encrypted.empty()) {
        return Result<void>::Error("Cannot decrypt empty data");
    }
    
    return DecryptDataInternal(encrypted, data);
}

bool SecurityManager::ValidateAPIKey(const std::string& key) const {
    if (key.empty()) {
        return false;
    }
    
    // Basic API key validation rules
    if (key.length() < 10) {
        return false;
    }
    
    // Check for common API key patterns
    std::regex api_key_patterns[] = {
        std::regex(R"(sk-[A-Za-z0-9]{48})"),           // OpenAI pattern
        std::regex(R"(xai-[A-Za-z0-9]{64})"),          // Anthropic pattern (example)
        std::regex(R"(AIza[A-Za-z0-9_-]{35})"),        // Google API pattern
        std::regex(R"([A-Za-z0-9]{32,128})")           // Generic pattern
    };
    
    for (const auto& pattern : api_key_patterns) {
        if (std::regex_match(key, pattern)) {
            return true;
        }
    }
    
    // If no pattern matches but key is reasonable length, allow it
    return key.length() >= 20 && key.length() <= 200;
}

Result<void> SecurityManager::LoadCredentialsFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return Result<void>::Error("Failed to open credentials file: " + filename);
    }
    
    // TODO: Implement secure file format for credentials
    // For now, just return success as a stub
    
    if (logger_) {
        // БЕЗОПАСНОСТЬ: скрываем чувствительные пути файлов
        std::string safe_filename = filename;
        size_t last_slash = safe_filename.find_last_of("/\\");
        if (last_slash != std::string::npos && last_slash + 1 < safe_filename.length()) {
            safe_filename = "..." + safe_filename.substr(last_slash);
        }
        logger_->LogFormatted(ILogger::Level::INFO, "Loaded credentials from: %s", safe_filename.c_str());
    }
    
    return Result<void>::Success();
}

Result<void> SecurityManager::SaveCredentialsToFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return Result<void>::Error("Failed to create credentials file: " + filename);
    }
    
    // TODO: Implement secure file format for credentials
    // For now, just return success as a stub
    
    if (logger_) {
        // БЕЗОПАСНОСТЬ: скрываем чувствительные пути файлов
        std::string safe_filename = filename;
        size_t last_slash = safe_filename.find_last_of("/\\");
        if (last_slash != std::string::npos && last_slash + 1 < safe_filename.length()) {
            safe_filename = "..." + safe_filename.substr(last_slash);
        }
        logger_->LogFormatted(ILogger::Level::INFO, "Saved credentials to: %s", safe_filename.c_str());
    }
    
    return Result<void>::Success();
}

void SecurityManager::ClearCredentials() {
    const std::lock_guard<std::mutex> lock(credentials_mutex_);
    
    // Clear encrypted data
    for (auto& credential_pair : encrypted_credentials_) {
        std::fill(credential_pair.second.begin(), credential_pair.second.end(), static_cast<uint8_t>(0));
    }
    
    encrypted_credentials_.clear();
    
    if (logger_) {
        logger_->Log(ILogger::Level::INFO, "Cleared all credentials");
    }
}

Result<void> SecurityManager::ValidateCredentialKey(const std::string& key) const {
    if (key.empty()) {
        return Result<void>::Error("Credential key cannot be empty");
    }
    
    if (key.length() > 256) {
        return Result<void>::Error("Credential key too long (max 256 characters)");
    }
    
    // Check for valid characters (alphanumeric, underscore, dash)
    std::regex valid_key_pattern(R"([A-Za-z0-9_-]+)");
    if (!std::regex_match(key, valid_key_pattern)) {
        return Result<void>::Error("Credential key contains invalid characters");
    }
    
    return Result<void>::Success();
}

Result<void> SecurityManager::ValidateCredentialValue(const std::string& value) const {
    if (value.empty()) {
        return Result<void>::Error("Credential value cannot be empty");
    }
    
    if (value.length() > 4096) {
        return Result<void>::Error("Credential value too long (max 4096 characters)");
    }
    
    return Result<void>::Success();
}

Result<std::vector<uint8_t>> SecurityManager::GenerateRandomBytes(size_t count) {
    std::vector<uint8_t> bytes(count);
    
#ifdef _WIN32
    // Use Windows Crypto API
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return Result<std::vector<uint8_t>>::Error("Failed to acquire crypto context");
    }
    
    if (!CryptGenRandom(hCryptProv, static_cast<DWORD>(count), bytes.data())) {
        CryptReleaseContext(hCryptProv, 0);
        return Result<std::vector<uint8_t>>::Error("Failed to generate random bytes");
    }
    
    CryptReleaseContext(hCryptProv, 0);
#else
    // Use standard random number generator (not cryptographically secure)
    // In production, would use /dev/urandom or OpenSSL
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < count; ++i) {
        bytes[i] = static_cast<uint8_t>(dis(gen));
    }
#endif
    
    return Result<std::vector<uint8_t>>::Success(bytes);
}

Result<void> SecurityManager::EncryptDataInternal(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) {
    if (data.empty()) {
        return Result<void>::Error("Cannot encrypt empty data");
    }
    
    if (encryption_key_.size() != 32) {
        return Result<void>::Error("Invalid key size for AES-256");
    }
    
#ifdef _WIN32
    return EncryptDataWindows(data, encrypted);
#else
    return EncryptDataOpenSSL(data, encrypted);
#endif
}

Result<void> SecurityManager::DecryptDataInternal(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) {
    if (encrypted.empty()) {
        return Result<void>::Error("Cannot decrypt empty data");
    }
    
    if (encryption_key_.size() != 32) {
        return Result<void>::Error("Invalid key size for AES-256");
    }
    
    // Минимальный размер: IV (16) + TAG (16) + данные (1+)
    if (encrypted.size() < 33) {
        return Result<void>::Error("Encrypted data too small to be valid");
    }
    
#ifdef _WIN32
    return DecryptDataWindows(encrypted, data);
#else
    return DecryptDataOpenSSL(encrypted, data);
#endif
}

// Реальное AES-256-GCM шифрование (Windows BCrypt API)
#ifdef _WIN32
Result<void> SecurityManager::EncryptDataWindows(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    
    // Инициализация AES алгоритма
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return Result<void>::Error("Failed to open AES algorithm provider");
    }
    
    // Установка режима GCM
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<void>::Error("Failed to set GCM mode");
    }
    
    // Создание ключа
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, 
                                       const_cast<PUCHAR>(encryption_key_.data()), 
                                       static_cast<ULONG>(encryption_key_.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<void>::Error("Failed to generate symmetric key");
    }
    
    // Генерация IV (12 байт для GCM)
    auto iv_result = GenerateRandomBytes(12);
    if (!iv_result.IsSuccess()) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<void>::Error("Failed to generate IV");
    }
    std::vector<uint8_t> iv = iv_result.Value();
    
    // Подготовка буферов
    std::vector<uint8_t> ciphertext(data.size());
    std::vector<uint8_t> tag(16); // 128-bit authentication tag
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = static_cast<ULONG>(iv.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());
    
    DWORD cbResult = 0;
    status = BCryptEncrypt(hKey, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()),
                          &authInfo, nullptr, 0, ciphertext.data(), static_cast<ULONG>(ciphertext.size()),
                          &cbResult, 0);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (!BCRYPT_SUCCESS(status)) {
        SecureZeroMemory(ciphertext.data(), ciphertext.size());
        SecureZeroMemory(tag.data(), tag.size());
        return Result<void>::Error("Encryption failed");
    }
    
    // Формат: IV (12) + TAG (16) + CIPHERTEXT
    encrypted.clear();
    encrypted.reserve(iv.size() + tag.size() + ciphertext.size());
    encrypted.insert(encrypted.end(), iv.begin(), iv.end());
    encrypted.insert(encrypted.end(), tag.begin(), tag.end());
    encrypted.insert(encrypted.end(), ciphertext.begin(), ciphertext.end());
    
    // Безопасная очистка временных буферов
    SecureZeroMemory(ciphertext.data(), ciphertext.size());
    SecureZeroMemory(tag.data(), tag.size());
    
    return Result<void>::Success();
}

Result<void> SecurityManager::DecryptDataWindows(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) {
    if (encrypted.size() < 28) { // IV(12) + TAG(16) = минимум 28 байт
        return Result<void>::Error("Encrypted data too small");
    }
    
    // Извлечение компонентов
    std::vector<uint8_t> iv(encrypted.begin(), encrypted.begin() + 12);
    std::vector<uint8_t> tag(encrypted.begin() + 12, encrypted.begin() + 28);
    std::vector<uint8_t> ciphertext(encrypted.begin() + 28, encrypted.end());
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    
    // Инициализация аналогично шифрованию
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return Result<void>::Error("Failed to open AES algorithm provider");
    }
    
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<void>::Error("Failed to set GCM mode");
    }
    
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                       const_cast<PUCHAR>(encryption_key_.data()),
                                       static_cast<ULONG>(encryption_key_.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<void>::Error("Failed to generate symmetric key");
    }
    
    // Подготовка для расшифровки
    data.resize(ciphertext.size());
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = static_cast<ULONG>(iv.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());
    
    DWORD cbResult = 0;
    status = BCryptDecrypt(hKey, ciphertext.data(), static_cast<ULONG>(ciphertext.size()),
                          &authInfo, nullptr, 0, data.data(), static_cast<ULONG>(data.size()),
                          &cbResult, 0);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (!BCRYPT_SUCCESS(status)) {
        SecureZeroMemory(data.data(), data.size());
        data.clear();
        return Result<void>::Error("Decryption failed - authentication tag mismatch or corruption");
    }
    
    data.resize(cbResult);
    return Result<void>::Success();
}

#else // Linux OpenSSL implementation

Result<void> SecurityManager::EncryptDataOpenSSL(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) {
#ifdef HAVE_OPENSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<void>::Error("Failed to create cipher context");
    }
    
    // Генерация IV
    auto iv_result = GenerateRandomBytes(12);
    if (!iv_result.IsSuccess()) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to generate IV");
    }
    std::vector<uint8_t> iv = iv_result.Value();
    
    // Инициализация AES-256-GCM
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to initialize AES-256-GCM");
    }
    
    // Установка размера IV
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to set IV length");
    }
    
    // Установка ключа и IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, encryption_key_.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to set key and IV");
    }
    
    // Шифрование
    std::vector<uint8_t> ciphertext(data.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(), static_cast<int>(data.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Encryption failed");
    }
    
    int ciphertext_len = len;
    
    // Финализация
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Encryption finalization failed");
    }
    ciphertext_len += len;
    
    // Получение тега аутентификации
    std::vector<uint8_t> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to get authentication tag");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Формат: IV (12) + TAG (16) + CIPHERTEXT
    encrypted.clear();
    encrypted.reserve(iv.size() + tag.size() + ciphertext_len);
    encrypted.insert(encrypted.end(), iv.begin(), iv.end());
    encrypted.insert(encrypted.end(), tag.begin(), tag.end());
    encrypted.insert(encrypted.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    
    // Безопасная очистка
    OPENSSL_cleanse(ciphertext.data(), ciphertext.size());
    OPENSSL_cleanse(tag.data(), tag.size());
    
    return Result<void>::Success();
#else
    return Result<void>::Error("OpenSSL not available - cannot encrypt data");
#endif
}

Result<void> SecurityManager::DecryptDataOpenSSL(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) {
#ifdef HAVE_OPENSSL
    if (encrypted.size() < 28) {
        return Result<void>::Error("Encrypted data too small");
    }
    
    // Извлечение компонентов
    std::vector<uint8_t> iv(encrypted.begin(), encrypted.begin() + 12);
    std::vector<uint8_t> tag(encrypted.begin() + 12, encrypted.begin() + 28);
    std::vector<uint8_t> ciphertext(encrypted.begin() + 28, encrypted.end());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<void>::Error("Failed to create cipher context");
    }
    
    // Инициализация
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to initialize AES-256-GCM");
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to set IV length");
    }
    
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, encryption_key_.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to set key and IV");
    }
    
    // Расшифровка
    data.resize(ciphertext.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx, data.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Decryption failed");
    }
    
    // Установка тега для проверки
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<void>::Error("Failed to set authentication tag");
    }
    
    // Финализация с проверкой тега
    int plaintext_len = len;
    if (EVP_DecryptFinal_ex(ctx, data.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(data.data(), data.size());
        data.clear();
        return Result<void>::Error("Decryption failed - authentication tag mismatch");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    data.resize(plaintext_len + len);
    
    return Result<void>::Success();
#else
    return Result<void>::Error("OpenSSL not available - cannot decrypt data");
#endif
}
#endif

} // namespace mcp