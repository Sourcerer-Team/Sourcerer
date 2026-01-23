function Controller() {
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore);
}

Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton, 3000);
}

Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.IntroductionPageCallback = function() {
    // Check if Sourcerer is running
    if (installer.isProcessRunning("Sourcerer.exe")) {
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

Controller.prototype.TargetDirectoryPageCallback = function() {
    gui.currentPageWidget().TargetDirectoryLineEdit.setText(installer.value("TargetDir"));
    gui.clickButton(buttons.NextButton);
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
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.PerformInstallationPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
    var checkBoxForm = gui.currentPageWidget().LaunchSourcererCheckBox;
    if (checkBoxForm && checkBoxForm.checked) {
        installer.executeDetached(installer.value("TargetDir") + "/Sourcerer.exe");
    }
    gui.clickButton(buttons.FinishButton);
}
