#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace mcp {

class ILogger;

class SecurityManager : public ISecurityManager {
public:
    explicit SecurityManager(std::shared_ptr<ILogger> logger);
    ~SecurityManager() override;

    // ISecurityManager implementation
    Result<void> StoreCredential(const std::string& key, const std::string& value) override;
    Result<std::string> RetrieveCredential(const std::string& key) override;
    Result<void> EncryptData(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) override;
    Result<void> DecryptData(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) override;
    bool ValidateAPIKey(const std::string& key) const override;

    // Extended functionality
    Result<void> LoadCredentialsFromFile(const std::string& filename);
    Result<void> SaveCredentialsToFile(const std::string& filename);
    Result<void> InitializeEncryption(const std::string& master_key);
    void ClearCredentials();

private:
    std::shared_ptr<ILogger> logger_;
    mutable std::mutex credentials_mutex_;
    
    // In-memory credential storage (encrypted)
    std::unordered_map<std::string, std::vector<uint8_t>> encrypted_credentials_;
    
    // Encryption state
    bool encryption_initialized_ = false;
    std::vector<uint8_t> encryption_key_;
    std::vector<uint8_t> encryption_iv_;
    
    // Credential validation
    Result<void> ValidateCredentialKey(const std::string& key) const;
    Result<void> ValidateCredentialValue(const std::string& value) const;
    
    // Encryption helpers
    Result<std::vector<uint8_t>> GenerateRandomBytes(size_t count);
    Result<std::vector<uint8_t>> DeriveKey(const std::string& password, const std::vector<uint8_t>& salt);
    
    // Platform-specific encryption
    Result<void> EncryptDataInternal(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted);
    Result<void> DecryptDataInternal(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data);
    
#ifdef _WIN32
    // Windows BCrypt API implementation
    Result<void> EncryptDataWindows(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted);
    Result<void> DecryptDataWindows(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data);
#else
    // Linux OpenSSL implementation
    Result<void> EncryptDataOpenSSL(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted);
    Result<void> DecryptDataOpenSSL(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data);
#endif
};

} // namespace mcp