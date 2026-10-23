# Git Encrypted Credential Storage

## Overview

This implementation provides secure, per-repository encrypted credential storage for Git operations in Sourcerer. Credentials are encrypted using AES-256 encryption and stored securely on disk with user-only file permissions.

## Features

- **Per-Repository Storage**: Credentials are stored separately for each repository path
- **AES-256 Encryption**: Industry-standard encryption using OpenSSL
- **PBKDF2 Key Derivation**: Repository-specific keys derived from master key using PBKDF2 with 100,000 iterations
- **Automatic Expiration**: Credentials expire after a configurable timeout (default: 30 minutes)
- **Optional Persistence**: Users can choose to remember credentials or use them for the session only
- **Secure File Permissions**: Credential files are created with user-only read/write permissions (0600)
- **Memory Protection**: Sensitive data is cleared from memory when credentials are removed

## Architecture

### Components

1. **GitCredentialStorage** (`lib/GitRepo/include/GitCredentialStorage.h`, `lib/GitRepo/src/GitCredentialStorage.cpp`)
   - Singleton class managing encrypted credential storage
   - Handles encryption/decryption, persistence, and memory management

2. **GitRepo Integration** (`lib/GitRepo/src/GitRepoCredentials.cpp`)
   - Modified credential callback to use encrypted storage
   - Automatically stores credentials when user chooses to remember them
   - Retrieves stored credentials before prompting user

3. **GitCredentialsResponse** (`lib/GitRepo/include/GitRepoTypes.h`)
   - Extended with `rememberCredentials` boolean field
   - Allows UI to indicate user's preference for credential persistence

## Storage Location

Credentials are stored in: `~/.local/share/Sourcerer/git_credentials.dat` (Linux)

The storage file format:
```
Version (int32)
Count (int32)
For each credential:
  - Repository Path (QString)
  - Encrypted Data (QByteArray)
    - Contains: username, password, creation timestamp
```

## Encryption Details

### Master Key
- 256-bit random key generated on first use
- Stored in application settings as hex string
- Used to derive repository-specific keys

### Repository-Specific Keys
- Derived using PBKDF2-HMAC-SHA256
- Salt: SHA-256 hash of repository path
- Iterations: 100,000
- Output: 256-bit key

### Encryption Process
1. Generate random 128-bit IV for each encryption
2. Encrypt data using AES-256-CBC
3. Prepend IV to ciphertext
4. Store encrypted data in credentials file

### Decryption Process
1. Extract IV from first 16 bytes
2. Decrypt remaining data using AES-256-CBC
3. Parse decrypted data to extract credentials

## API Usage

### Storing Credentials

```cpp
#include "GitCredentialStorage.h"

GitRepo::GitCredentialStorage &storage = 
    GitRepo::GitCredentialStorage::getInstance();

// Store with persistence
storage.storeCredentials("/path/to/repo", "username", "password", true);

// Store for session only
storage.storeCredentials("/path/to/repo", "username", "password", false);
```

### Retrieving Credentials

```cpp
QString username, password;
if (storage.getCredentials("/path/to/repo", username, password)) {
    // Use credentials
}
```

### Checking Expiration

```cpp
qint64 expirationTimeMs = 30 * 60 * 1000; // 30 minutes
if (storage.areCredentialsExpired("/path/to/repo", expirationTimeMs)) {
    // Credentials expired, need to re-acquire
}
```

### Removing Credentials

```cpp
// Remove specific repository credentials
storage.removeCredentials("/path/to/repo");

// Clear all in-memory credentials (persisted remain on disk)
storage.clearMemoryCredentials();

// Clear everything including persisted credentials
storage.clearAll();
```

## Integration with Git Operations

### Clone Operation
```cpp
GitCredentialPayload credPayload;
credPayload.repoPath = destinationDir;
credPayload.uuid = uuid;

fetch_opts.callbacks.credentials = &credentialsAcquireCallback;
fetch_opts.callbacks.payload = &credPayload;
```

### Pull/Push Operations
```cpp
QString repoPath = extractRepoPath(repo);

GitCredentialPayload credPayload;
credPayload.repoPath = repoPath;

fetchOpts.callbacks.credentials = &credentialsAcquireCallback;
fetchOpts.callbacks.payload = &credPayload;
```

### Credential Callback Flow

1. **Check Stored Credentials**: First attempts to retrieve from encrypted storage
2. **Check Expiration**: Validates credentials haven't expired
3. **Return Stored**: If valid credentials exist, returns them immediately
4. **Request from User**: If no valid credentials, prompts user via event system
5. **Store on Success**: If user chooses "remember", stores credentials encrypted

## Security Considerations

### Strengths
- Strong encryption (AES-256 with PBKDF2 key derivation)
- Per-repository encryption keys
- Secure file permissions (user-only access)
- Automatic credential expiration
- Memory cleared on removal

### Limitations
- Master key stored in application settings (encrypted by Qt/OS)
- Passwords stored in memory during use (required for Git operations)
- No password for master key (relies on OS user authentication)
- Single-user system (no multi-user credential isolation)

### Recommendations
- Use SSH keys for Git operations when possible
- Enable full-disk encryption on the system
- Use short expiration timeouts for sensitive repositories
- Clear credentials when not actively working

## Configuration

### Expiration Timeout
Configure in `ConfigManager`:
```cpp
ConfigKey::GitRepoGitCredentialsExpirationTime
```
Default: 30 minutes (1,800,000 milliseconds)

## UI Integration

The UI should be modified to:
1. Add "Remember credentials" checkbox to credential dialog
2. Set `GitCredentialsResponse.rememberCredentials` based on user choice
3. Provide UI to manage stored credentials (view, delete)
4. Allow user to clear all stored credentials from settings

## Dependencies

- **OpenSSL**: For AES encryption and PBKDF2 key derivation
- **Qt6 Core**: For data structures, file I/O, and serialization
- **ConfigManager**: For storing master key and configuration
- **EventManager**: For credential request/response flow

## Build Configuration

Added to `lib/GitRepo/CMakeLists.txt`:
```cmake
find_package(OpenSSL REQUIRED)

target_link_libraries(GitRepo PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)
```

## Testing

### Manual Testing
1. Clone a repository requiring credentials
2. Enter credentials with "Remember" checked
3. Perform another Git operation (pull/push)
4. Verify credentials are retrieved automatically
5. Wait for expiration timeout
6. Verify credentials are re-requested after expiration
7. Check file permissions: `ls -l ~/.local/share/Sourcerer/git_credentials.dat`

### Security Testing
1. Verify credential file is encrypted (not plain text)
2. Verify file permissions are 0600 (user read/write only)
3. Verify credentials expire after configured timeout
4. Verify credentials are cleared from memory on removal

## Future Enhancements

1. **OS Keychain Integration**: Use system keychain (Keyring, Keychain Access)
2. **Master Password**: Optional password protection for master key
3. **Credential Sharing**: Share credentials across related repositories
4. **Audit Logging**: Log credential access and modifications
5. **Biometric Authentication**: Support fingerprint/face authentication
6. **Remote Credential Sync**: Securely sync credentials across devices
7. **Multi-Factor Authentication**: Support for 2FA tokens

## Migration from Old System

The old in-memory credential system (`gGitCredentials`) is still used as a cache. The new system:
- Checks encrypted storage first
- Falls back to in-memory cache
- Stores new credentials in both systems
- Gradually deprecates in-memory-only storage

No migration is needed - credentials will be re-entered and stored encrypted on next use.

## Troubleshooting

### Credentials Not Saving
- Check file permissions on `git_credentials.dat`
- Verify OpenSSL is properly linked
- Check logs for encryption errors

### Credentials Not Loading
- Verify credential file exists and is readable
- Check for corruption (remove file to reset)
- Verify repository path matches exactly

### Permission Denied
- Check file owner matches current user
- Verify file permissions (should be 0600)
- Check parent directory permissions

## Support

For issues or questions, please file an issue in the Sourcerer repository with the "credentials" label.
