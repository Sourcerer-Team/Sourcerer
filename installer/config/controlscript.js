function Controller() {
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore);
    installer.setMessageBoxAutomaticAnswer("installationErrorWithRetry", QMessageBox.Ignore);
    installer.setMessageBoxAutomaticAnswer("TargetDirectoryInUse", QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer("OperationDoesNotExistError", QMessageBox.Ignore);
    this.uninstallPerformed = false;
}

Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton, 3000);
}

Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.IntroductionPageCallback = function() {
    // Don't check for updates if we're already in maintenance/uninstall mode
    if (installer.isUninstaller() || installer.isUpdater() || installer.isPackageManager()) {
        gui.clickButton(buttons.NextButton);
        return;
    }
    
    // Check if previous installation exists (but only prompt once)
    var targetDir = installer.value("TargetDir");
    var maintenanceTool = targetDir + "/MaintenanceTool";
    if (systemInfo.productType === "windows") {
        maintenanceTool = targetDir + "/MaintenanceTool.exe";
    }
    
    if (!this.uninstallPerformed && installer.fileExists(maintenanceTool)) {
        var result = QMessageBox.question("update.installation", "Existing Installation Found", 
            "Sourcerer is already installed. Would you like to update it to the new version?\n\n" +
            "The existing installation will be removed and replaced with the new version.",
            QMessageBox.Yes | QMessageBox.Cancel,
            QMessageBox.Yes);
        
        if (result == QMessageBox.Yes) {
            // Check if Sourcerer is running in a loop until user closes it or cancels
            var processName = systemInfo.productType === "windows" ? "Sourcerer.exe" : "Sourcerer";
            while (installer.isProcessRunning(processName)) {
                var closeResult = QMessageBox.question("app.running", "Application Running",
                    "Sourcerer is currently running and must be closed before updating.\n\n" +
                    "Would you like to terminate it automatically?",
                    QMessageBox.Yes | QMessageBox.No | QMessageBox.Cancel,
                    QMessageBox.Yes);
                
                if (closeResult == QMessageBox.Yes) {
                    // Terminate the process automatically
                    console.log("Terminating " + processName + "...");
                    
                    // Use system commands to kill the process
                    try {
                        if (systemInfo.productType === "windows") {
                            installer.execute("taskkill", ["/F", "/IM", processName]);
                        } else {
                            // Use pkill with exact match to avoid killing the installer itself
                            // -x means exact match of the process name
                            installer.execute("pkill", ["-9", "-x", "Sourcerer"]);
                        }
                    } catch(e) {
                        console.log("Error killing process: " + e);
                    }
                    
                    // Wait a moment for process to terminate
                    installer.performOperation("Wait", "2000");
                    
                    // Check if it's still running
                    if (installer.isProcessRunning(processName)) {
                        QMessageBox.warning("terminate.failed", "Termination Failed",
                            "Failed to terminate Sourcerer. Please close it manually.",
                            QMessageBox.Ok);
                    } else {
                        break; // Process terminated successfully, exit loop
                    }
                } else if (closeResult == QMessageBox.No) {
                    // User wants to close manually, wait and retry
                    var retryResult = QMessageBox.question("close.manually", "Close Application",
                        "Please close Sourcerer manually, then click 'OK' to continue.",
                        QMessageBox.Ok | QMessageBox.Cancel,
                        QMessageBox.Ok);
                    
                    if (retryResult == QMessageBox.Cancel) {
                        gui.clickButton(buttons.CancelButton);
                        return;
                    }
                    // Loop continues to check again
                } else {
                    // Cancel installation
                    gui.clickButton(buttons.CancelButton);
                    return;
                }
            }
            
            // Mark that we've performed the cleanup
            this.uninstallPerformed = true;
            
            // Run MaintenanceTool purge to clean up the installation
            console.log("Running MaintenanceTool purge: " + maintenanceTool);
            var removeSuccess = false;
            try {
                if (installer.fileExists(maintenanceTool)) {
                    var exitCode = installer.execute(maintenanceTool, ["purge", "-c"]);
                    console.log("MaintenanceTool purge exit code: " + exitCode);
                    
                    // Wait longer for filesystem to update and processes to exit
                    installer.performOperation("Wait", "3000");
                    
                    // Check if directory was actually removed
                    if (!installer.fileExists(targetDir) || !installer.fileExists(maintenanceTool)) {
                        console.log("Installation successfully purged");
                        removeSuccess = true;
                    } else {
                        console.log("Purge completed but some files remain, checking exit code");
                        // If exit code is 0 or 1, consider it success anyway
                        if (exitCode === 0 || exitCode === 1) {
                            console.log("Exit code indicates success despite remaining files");
                            removeSuccess = true;
                        }
                    }
                } else {
                    console.log("MaintenanceTool not found, cannot purge");
                }
            } catch(e) {
                console.log("Error running MaintenanceTool purge: " + e);
            }
            
            // Check if removal failed
            if (!removeSuccess) {
                // Before failing, check one more time if directory exists
                var stillExists = installer.fileExists(targetDir);
                console.log("Final check - directory still exists: " + stillExists);
                
                if (!stillExists) {
                    // Directory is gone, consider it success
                    console.log("Directory removed despite removeSuccess flag being false");
                    removeSuccess = true;
                } else {
                    // Try one more time with manual registry and file cleanup
                    if (systemInfo.productType === "windows") {
                        console.log("Purge failed, attempting manual cleanup...");
                        try {
                            // Remove registry entries manually
                            var registryPaths = [
                                "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Sourcerer",
                                "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Sourcerer",
                                "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Sourcerer"
                            ];
                            
                            for (var i = 0; i < registryPaths.length; i++) {
                                installer.execute("reg", ["delete", registryPaths[i], "/f"]);
                            }
                            
                            // Try to remove the directory with cmd
                            installer.execute("cmd.exe", ["/c", "rmdir", "/s", "/q", '"' + targetDir + '"']);
                            
                            // Wait and check one final time
                            installer.performOperation("Wait", "1000");
                            
                            if (!installer.fileExists(targetDir)) {
                                console.log("Manual cleanup succeeded");
                                removeSuccess = true;
                            } else {
                                console.log("Manual cleanup failed");
                            }
                        } catch(e) {
                            console.log("Warning: Manual cleanup failed: " + e);
                        }
                    }
                }
            }
            
            if (!removeSuccess) {
                // Even if purge failed, let's try to continue anyway
                // The installer can overwrite files
                console.log("Warning: Uninstall may not have completed fully, but continuing with installation");
                QMessageBox.information("update.warning", "Update Notice",
                    "The old installation could not be completely removed, but the installer will attempt to update the files.\n\n" +
                    "If you encounter any issues, please manually delete the installation directory and try again.",
                    QMessageBox.Ok);
            }
            
            // Continue with installation regardless
            gui.clickButton(buttons.NextButton);
        } else {
            // Cancel installation
            gui.clickButton(buttons.CancelButton);
        }
    } else {
        // No previous installation, check if app is running
        var processName = systemInfo.productType === "windows" ? "Sourcerer.exe" : "Sourcerer";
        if (installer.isProcessRunning(processName)) {
            var result = QMessageBox.question("quit.question", "Installer", 
                "Sourcerer is currently running. Please close it before continuing with the installation.",
                QMessageBox.Ok | QMessageBox.Cancel);
            if (result == QMessageBox.Ok) {
                gui.clickButton(buttons.NextButton);
            } else {
                gui.clickButton(buttons.CancelButton);
            }
        } else {
            gui.clickButton(buttons.NextButton);
        }
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var page = gui.currentPageWidget();
    if (page) {
        page.TargetDirectoryLineEdit.setText(installer.value("TargetDir"));
    }
    // Let user choose the directory - don't auto-click Next
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.LicenseAgreementPageCallback = function() {
    gui.currentPageWidget().AcceptLicenseRadioButton.setChecked(true);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.StartMenuDirectoryPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    // Don't automatically start installation - let user click Install button manually
}

Controller.prototype.PerformInstallationPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
    var checkBoxForm = gui.currentPageWidget().LaunchSourcererCheckBox;
    if (checkBoxForm && checkBoxForm.checked) {
        installer.executeDetached(installer.value("TargetDir") + "/Sourcerer.exe");
    }
    // Don't automatically close - let user click Finish button manually
}
