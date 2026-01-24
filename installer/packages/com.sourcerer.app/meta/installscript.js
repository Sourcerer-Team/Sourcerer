function Component()
{
    // constructor
    // Check if this is an update
    if (installer.isUpdater()) {
        console.log("Update mode detected");
    } else if (installer.isUninstaller()) {
        console.log("Uninstall mode detected");
    } else {
        console.log("Install mode detected");
    }
}

Component.prototype.createOperations = function()
{
    // call default implementation to actually install the component
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut", "@TargetDir@/Sourcerer.exe", "@StartMenuDir@/Sourcerer.lnk",
            "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/Sourcerer.exe", "iconId=0", 
            "description=Launch Sourcerer");
        component.addOperation("CreateShortcut", "@TargetDir@/Sourcerer.exe", "@DesktopDir@/Sourcerer.lnk",
            "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/Sourcerer.exe", "iconId=0",
            "description=Launch Sourcerer");
    }
    
    if (systemInfo.productType === "osx") {
        // macOS specific operations
        component.addOperation("Execute", "ln", "-sf", "@TargetDir@/Sourcerer.app", "/Applications/Sourcerer.app");
    }
    
    if (systemInfo.productType !== "windows" && systemInfo.productType !== "osx") {
        // Linux specific operations
        
        // Check if we're updating and backup user data if needed
        var isUpdate = installer.isUpdater();
        var targetDir = installer.value("TargetDir");
        var homeDir = installer.value("HomeDir");
        var appVersion = installer.value("Version");
        
        console.log("Installation path: " + targetDir);
        console.log("Home directory: " + homeDir);
        console.log("Application version: " + appVersion);
        console.log("Is update: " + isUpdate);
        
        // Ensure .local/share/applications directory exists
        component.addOperation("Execute", 
            "mkdir", "-p", homeDir + "/.local/share/applications");
        
        // Create .desktop file with correct paths using echo
        var desktopContent = "[Desktop Entry]\\n" +
            "Version=" + appVersion + "\\n" +
            "Type=Application\\n" +
            "Name=Sourcerer\\n" +
            "Comment=Git Repository Viewer and Manager\\n" +
            "Exec=" + targetDir + "/Sourcerer-launcher.sh\\n" +
            "Icon=" + targetDir + "/sourcerer.svg\\n" +
            "Terminal=false\\n" +
            "Categories=Development;RevisionControl;\\n" +
            "StartupNotify=true\\n" +
            "StartupWMClass=Sourcerer";
        
        component.addOperation("Execute", 
            "sh", "-c", 
            "echo -e '" + desktopContent + "' > " + homeDir + "/.local/share/applications/sourcerer.desktop");
        
        // Make the launcher script executable
        component.addOperation("Execute", 
            "chmod", "+x", targetDir + "/Sourcerer-launcher.sh");
        
        // Make the main executable executable
        component.addOperation("Execute", 
            "chmod", "+x", targetDir + "/Sourcerer");
        
        // Update desktop database to reflect new/updated .desktop file
        // Use sh -c to ignore errors if update-desktop-database is not installed
        component.addOperation("Execute", 
            "sh", "-c", 
            "update-desktop-database '" + homeDir + "/.local/share/applications' 2>/dev/null || true");
    }
}

// Called when uninstalling
Component.prototype.createOperationsForArchive = function(archive)
{
    component.createOperationsForArchive(archive);
}
