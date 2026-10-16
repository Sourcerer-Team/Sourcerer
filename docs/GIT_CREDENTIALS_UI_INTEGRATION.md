# Git Credentials UI Integration - Implementation Summary

## Overview

This document describes the UI integration for the encrypted Git credentials storage system. The implementation adds a "Store credentials" checkbox to the Git credentials overlay, allowing users to save their credentials securely for future use.

## Changes Made

### 1. Data Structure Updates

#### File: `lib/GitRepo/include/GitRepo.h`
- **Added**: `QString repoPath` field to `GitCredentialsRequest` struct
- **Purpose**: Pass repository path through the event system to the UI layer

```cpp
struct GitCredentialsRequest {
  size_t id = -1;
  QString url;
  QString username;
  int allowedTypes;
  QString repoPath;  // NEW FIELD
};
```

### 2. Backend Integration

#### File: `lib/GitRepo/src/GitRepo.cpp`
- **Modified**: `acquireCredentials()` function
- **Change**: Set `request.repoPath = repoPath` when creating the credentials request
- **Line**: ~1257

```cpp
GitCredentialsRequest request;
request.id = qHash(url);
request.url = url;
request.username = username;
request.allowedTypes = allowedTypes;
request.repoPath = repoPath;  // NEW LINE
```

### 3. UI Widget Updates

#### File: `component/GuiManager/include/widgets/GitCredentialsOverlayWidget.h`

**Added Members:**
```cpp
QCheckBox *_storeCredentialsCheckBox;  // Checkbox widget
QString _repoPath;                      // Repository path for credential storage
```

**Added Method:**
```cpp
void setRepoPath(const QString &repoPath);  // Sets repository path before showing overlay
```

#### File: `component/GuiManager/src/widgets/GitCredentialsOverlayWidget.cpp`

**Constructor Changes:**
1. Created `_storeCredentialsCheckBox` with text "Store credentials"
2. Set pointing hand cursor for better UX
3. Increased group box height from 100 to 130 pixels to accommodate checkbox
4. Added checkbox to layout with proper spacing

**setRequest() Method:**
- Loads stored credentials from `GitCredentialStorage` if `_repoPath` is set
- Auto-populates username and password fields with stored values
- Sets checkbox to checked state if credentials were loaded
- Falls back to request username if no stored credentials exist

```cpp
void GitCredentialsOverlayWidget::setRequest(const GitCredentialsRequest &request) {
  _request = request;
  
  // Try to load credentials from storage
  QString storedUsername, storedPassword;
  bool hasStoredCredentials = false;
  
  if (!_repoPath.isEmpty()) {
    GitRepo::GitCredentialStorage &storage = GitRepo::GitCredentialStorage::getInstance();
    if (storage.getCredentials(_repoPath, storedUsername, storedPassword)) {
      hasStoredCredentials = true;
      _usernameLineEdit->setText(storedUsername);
      _passwordLineEdit->setText(storedPassword);
      _storeCredentialsCheckBox->setChecked(true);
    }
  }
  
  if (!hasStoredCredentials) {
    _usernameLineEdit->setText(request.username);
    _passwordLineEdit->setText(QString());
    _storeCredentialsCheckBox->setChecked(false);
  }
  
  _urlLabel->setText(request.url);
}
```

**setRepoPath() Method:**
- New method to set repository path before displaying the overlay
- Must be called before `setRequest()` for credential loading to work

```cpp
void GitCredentialsOverlayWidget::setRepoPath(const QString &repoPath) {
  _repoPath = repoPath;
}
```

**onSubmitClicked() Method:**
- Reads checkbox state and sets `response.rememberCredentials`
- GitRepo layer will handle actual credential storage based on this flag

```cpp
void GitCredentialsOverlayWidget::onSubmitClicked() const {
  GitRepo::GitCredentialsResponse response;
  response.id = _request.id;
  response.username = _usernameLineEdit->text();
  response.password = _passwordLineEdit->text();
  response.rememberCredentials = _storeCredentialsCheckBox->isChecked();  // NEW LINE
  
  // Send response event...
}
```

### 4. MainWindow Integration

#### File: `component/GuiManager/src/MainWindow.cpp`
- **Modified**: `showCredentialsOverlay()` method
- **Change**: Call `widget->setRepoPath(request.repoPath)` before `widget->setRequest(request)`
- **Line**: ~877

```cpp
void MainWindow::showCredentialsOverlay(const GitCredentialsRequest &request,
                                        GitCredentialsOverlayWidget *widget) {
  Q_ASSERT(widget != nullptr);

  if (!widget) {
    return;
  }

  widget->setRepoPath(request.repoPath);  // NEW LINE - Must be called first
  widget->setRequest(request);
  widget->start();
  widget->show();
  widget->raise();
}
```

## User Flow

### First-Time Credential Entry
1. User performs Git operation (clone, pull, push) requiring authentication
2. Git credentials overlay appears with empty fields
3. User enters username and password
4. User checks "Store credentials" checkbox (optional)
5. User clicks Submit
6. If checked, credentials are encrypted and stored using AES-256
7. Operation proceeds with provided credentials

### Subsequent Operations on Same Repository
1. User performs Git operation requiring authentication
2. System checks `GitCredentialStorage` for saved credentials
3. If found and not expired:
   - Credentials are decrypted and used automatically
   - No overlay shown
4. If not found or expired:
   - Overlay appears with previously stored credentials pre-filled
   - "Store credentials" checkbox is checked
   - User can modify credentials or uncheck to not update storage
   - User clicks Submit to proceed

## Security Features

### Checkbox Integration with Encryption
- Checkbox state determines whether credentials persist
- When checked: Credentials encrypted with AES-256-CBC before storage
- When unchecked: No persistent storage, credentials used only for current operation
- Credentials stored per repository using repository-specific encryption keys

### Automatic Expiration
- Stored credentials automatically expire after configured timeout
- Default: 30 days (configurable via `GitRepoGitCredentialsExpirationTime`)
- Expired credentials trigger new credential request with pre-filled values

### Memory Security
- Passwords cleared from memory after encryption
- QString instances overwritten with zeros before deallocation
- No plaintext credentials in logs

## Configuration

The following settings control credential storage behavior:

```cpp
// In ConfigManager
ConfigKey::GitRepoGitCredentialsExpirationTime  // Timeout in milliseconds
ConfigKey::GitRepoGitCredentialsStorageEnabled  // Master enable/disable flag
```

## Testing Checklist

- [ ] Checkbox appears in Git credentials overlay
- [ ] Checkbox has correct label "Store credentials"
- [ ] Checkbox shows pointing hand cursor on hover
- [ ] Entering credentials with checkbox checked stores them encrypted
- [ ] Entering credentials with checkbox unchecked doesn't store them
- [ ] Stored credentials auto-populate on subsequent requests
- [ ] Checkbox is checked when credentials are auto-populated
- [ ] Unchecking checkbox and submitting removes stored credentials
- [ ] Expired credentials trigger new request with pre-filled values
- [ ] Multiple repositories maintain separate credential stores
- [ ] Credential storage survives application restart
- [ ] Credentials work for clone, pull, and push operations

## Files Modified

1. `lib/GitRepo/include/GitRepo.h` - Added `repoPath` to `GitCredentialsRequest`
2. `lib/GitRepo/src/GitRepo.cpp` - Set `repoPath` in credential request
3. `component/GuiManager/include/widgets/GitCredentialsOverlayWidget.h` - Added checkbox member and `setRepoPath()` method
4. `component/GuiManager/src/widgets/GitCredentialsOverlayWidget.cpp` - Implemented checkbox UI and storage integration
5. `component/GuiManager/src/MainWindow.cpp` - Call `setRepoPath()` before showing overlay

## Dependencies

- **Qt6 Widgets**: QCheckBox for UI control
- **GitCredentialStorage**: Backend encryption and storage system (see GIT_CREDENTIALS.md)
- **OpenSSL**: AES-256-CBC encryption (already configured)

## Related Documentation

- `GIT_CREDENTIALS.md` - Complete backend encryption system documentation
- `lib/GitRepo/include/GitCredentialStorage.h` - API documentation

## Future Enhancements

1. **Credential Manager UI**: Separate dialog to view/edit/delete stored credentials
2. **Per-Site Timeout**: Different expiration times per repository
3. **Credential Sharing**: Share credentials across repositories from same host
4. **Biometric Auth**: Platform-specific biometric authentication before revealing credentials
5. **Import/Export**: Backup and restore encrypted credential database
