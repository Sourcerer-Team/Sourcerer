# BugReportContainer Implementation Guide

## Overview

The BugReportContainer provides a user-friendly interface for submitting bug reports with automatic log file attachment. It includes progress tracking and uses HTTP multipart/form-data for efficient file uploads.

## Features

1. **Form Input**
   - Bug title (single line)
   - Bug description (multi-line text area)
   - Input validation

2. **Automatic Log Collection**
   - Automatically collects all `.log` files from the logs directory
   - Creates a zip archive for efficient upload
   - Handles cases where no log files exist

3. **Upload Progress**
   - Spinner overlay with semi-transparent background
   - Percentage progress display (0-100%)
   - Visual feedback during upload

4. **Error Handling**
   - Network error handling
   - Toast notifications for success/failure
   - Graceful degradation if logs unavailable

## Architecture

### Most Efficient Upload Approach

The implementation uses **QNetworkAccessManager with HTTP multipart/form-data**, which is the most efficient approach because:

1. **Single HTTP Request**: All data (title, description, and zip file) sent in one request
2. **Streaming Upload**: Files are streamed, not loaded entirely into memory
3. **Built-in Progress Tracking**: Qt's signals provide real-time upload progress
4. **Standard Format**: multipart/form-data is universally supported by REST APIs
5. **Automatic Content-Type Handling**: Qt manages proper headers and boundaries

### Alternative Approaches (Not Recommended)

- **Base64 Encoding**: Would increase payload size by ~33% and require more memory
- **Multiple Requests**: Separate requests for metadata and file would be slower
- **FTP Upload**: Requires separate server setup and lacks progress callbacks

## Dependencies

### Required Qt Modules
```cmake
find_package(Qt6 REQUIRED COMPONENTS Network)
target_link_libraries(GuiManager PRIVATE Qt6::Network)
```

### ZIP Library (Choose One)

#### Option 1: QuaZIP (Recommended)
```cmake
find_package(QuaZIP-Qt6)
target_link_libraries(GuiManager PRIVATE QuaZIP::QuaZIP)
add_compile_definitions(USE_QUAZIP)
```

#### Option 2: libzip
```cmake
find_package(libzip)
target_link_libraries(GuiManager PRIVATE libzip::zip)
```

#### Option 3: System Command (Fallback)
Use system commands like `7z` or `zip` via QProcess (slower, less portable)

## Integration into MainWindow

### 1. Add to MainWindow.h

```cpp
#include "containers/BugReportContainer.h"

private slots:
  void onBugReport();

private:
  void addBugReportContainer();
```

### 2. Add to MainWindow.cpp

```cpp
void MainWindow::addBugReportContainer() {
  LOG_PERF(0, "Enter");

  const auto container = QSharedPointer<BugReportContainer>::create(this);
  container->selected();

  _containers += container;

  _topBarWidget->titleBarWidget()->addTab(tr("Bug Report"));
  _topBarWidget->titleBarWidget()->selectTab(
      _topBarWidget->titleBarWidget()->tabsCount() - 1);

  _centralWidget->setCurrentWidget(container.get());

  selectContainer(_containers.size() - 1);

  LOG_PERF(0, "Exit");
}

void MainWindow::onBugReport() {
  LOG_PERF(0, "Enter");
  addBugReportContainer();
  LOG_PERF(0, "Exit");
}
```

### 3. Add Menu Item

In TitleBarMenuButton.cpp:

```cpp
_bugReportAction = new QAction(this);
_bugReportAction->setText(tr("Report a &Bug"));
_bugReportAction->setShortcut(QKeySequence("Ctrl+B"));
connect(_bugReportAction, &QAction::triggered, this,
        &TitleBarMenuButton::onBugReport);

menu->addAction(_bugReportAction);
```

Add signal and slot:
```cpp
signals:
  void bugReport();

private slots:
  void onBugReport();
```

```cpp
void TitleBarMenuButton::onBugReport() {
  emit bugReport();
}
```

Connect in MainWindow constructor:
```cpp
connect(_topBarWidget, &TopBarWidget::bugReport, this,
        &MainWindow::onBugReport);
```

## Server-Side REST API

### Endpoint Specification

**URL**: `POST https://api.yourcompany.com/bug-reports`

**Content-Type**: `multipart/form-data`

### Request Fields

| Field | Type | Description |
|-------|------|-------------|
| `title` | text | Bug title (required) |
| `description` | text | Detailed bug description (required) |
| `logs` | file | ZIP file containing log files (optional) |

### Example Server Implementation (Node.js/Express)

```javascript
const express = require('express');
const multer = require('multer');
const fs = require('fs');

const upload = multer({ dest: 'uploads/' });

app.post('/bug-reports', upload.single('logs'), async (req, res) => {
  try {
    const { title, description } = req.body;
    const logFile = req.file;
    
    // Validate inputs
    if (!title || !description) {
      return res.status(400).json({ error: 'Title and description required' });
    }
    
    // Save to database
    const bugReport = await db.bugReports.create({
      title,
      description,
      logsPath: logFile ? logFile.path : null,
      submittedAt: new Date()
    });
    
    res.status(201).json({
      success: true,
      bugId: bugReport.id,
      message: 'Bug report received successfully'
    });
  } catch (error) {
    console.error('Error saving bug report:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});
```

### Response Format

**Success (HTTP 201)**:
```json
{
  "success": true,
  "bugId": "12345",
  "message": "Bug report received successfully"
}
```

**Error (HTTP 400/500)**:
```json
{
  "success": false,
  "error": "Error description"
}
```

## Configuration

### Update BugReportContainer.cpp

Change the endpoint URL:
```cpp
#define BUG_REPORT_ENDPOINT "https://your-actual-api.com/bug-reports"
```

### Optional: Add to ConfigManager

Add configuration key:
```cpp
// In ConfigKey.h
BugReportEndpoint,

// In ConfigManager
case ConfigKey::BugReportEndpoint:
  return "https://api.yourcompany.com/bug-reports";
```

Then use in BugReportContainer:
```cpp
QString endpoint = _configManager.getString(
    ConfigManager::ConfigKey::BugReportEndpoint);
```

## Testing

### 1. Test Without Server

Use a mock server like [RequestBin](https://requestbin.com/) or [httpbin](https://httpbin.org/):

```cpp
#define BUG_REPORT_ENDPOINT "https://httpbin.org/post"
```

### 2. Test Progress Simulation

Add to onSubmitClicked for testing:
```cpp
// Simulate progress for testing
QTimer *timer = new QTimer(this);
connect(timer, &QTimer::timeout, [this, timer, progress = 0]() mutable {
  progress += 10;
  updateProgressLabel(progress);
  if (progress >= 100) {
    timer->stop();
    timer->deleteLater();
  }
});
timer->start(200);
```

### 3. Test Error Handling

Temporarily use an invalid URL to test error handling:
```cpp
#define BUG_REPORT_ENDPOINT "https://invalid-url-for-testing.com/bug-reports"
```

## Build Configuration

Add to CMakeLists.txt:

```cmake
# Add BugReportContainer sources
set(CONTAINER_SOURCES
    # ... existing sources ...
    src/containers/BugReportContainer.cpp
    include/containers/BugReportContainer.h
)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Network)

# Optional: QuaZIP for zip compression
find_package(QuaZIP-Qt6)
if(QuaZIP-Qt6_FOUND)
    target_link_libraries(GuiManager PRIVATE QuaZIP::QuaZIP)
    target_compile_definitions(GuiManager PRIVATE USE_QUAZIP)
endif()

target_link_libraries(GuiManager PRIVATE Qt6::Network)
```

## Future Enhancements

1. **Email Field**: Add optional user email for follow-up
2. **Screenshot Attachment**: Allow users to attach screenshots
3. **System Info**: Automatically collect OS version, app version
4. **Offline Support**: Queue reports when network unavailable
5. **File Size Limit**: Warn if log files exceed size limit
6. **Compression Level**: Make compression level configurable
7. **Auto-submit Crashes**: Automatically submit crash reports
8. **Privacy Options**: Let users review logs before submission

## Troubleshooting

### Zip Creation Fails

**Problem**: `createLogsZipFile()` returns empty string

**Solutions**:
- Ensure QuaZIP is properly installed and linked
- Check if logs directory exists and contains .log files
- Verify write permissions to temp directory
- Check logs for specific error messages

### Upload Fails

**Problem**: Network error during upload

**Solutions**:
- Verify endpoint URL is correct and accessible
- Check firewall/proxy settings
- Test with curl: `curl -X POST -F "title=test" endpoint-url`
- Check server logs for errors
- Verify SSL certificates if using HTTPS

### Progress Not Updating

**Problem**: Progress stays at 0%

**Solutions**:
- Ensure `uploadProgress` signal is connected
- Check if file is being streamed (not preloaded)
- Verify `bytesTotal` is not zero
- Test with smaller files first

## Security Considerations

1. **Sanitize Inputs**: Server should validate and sanitize all inputs
2. **File Size Limits**: Set reasonable limits (e.g., 50MB max)
3. **Rate Limiting**: Prevent spam submissions
4. **Authentication**: Consider requiring user authentication
5. **HTTPS Only**: Always use HTTPS in production
6. **Log Privacy**: Ensure logs don't contain sensitive data
7. **Virus Scanning**: Scan uploaded files on server
