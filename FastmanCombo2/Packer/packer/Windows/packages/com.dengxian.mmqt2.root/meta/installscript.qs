/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

function Component()
{
    // constructor
    component.loaded.connect(this, Component.prototype.loaded);
    if (!installer.addWizardPage(component, "Page", QInstaller.TargetDirectory))
        console.log("Could not add the dynamic page.");
	
	// connect
	installer.installationFinished.connect(this, Component.prototype.installationFinishedPageIsShown);
    installer.finishButtonClicked.connect(this, Component.prototype.installationFinished);	
	
	gui.pageWidgetByObjectName("IntroductionPage").entered.connect(changeLicenseLabels);
	gui.pageWidgetByObjectName("ComponentSelectionPage").entered.connect(changeLicenseLabels);
	gui.pageWidgetByObjectName("ReadyForInstallationPage").entered.connect(changeLicenseLabels);
}

changeLicenseLabels = function()
{    
	var widget = gui.currentPageWidget();
    gui.clickButton(buttons.NextButton);
}

Component.prototype.isDefault = function()
{
    // select the component by default
    return true;
}

Component.prototype.createOperations = function()
{
    try {
        // call the base create operations function
        component.createOperations();
		
		if (systemInfo.productType === "windows") {
			// set RUNASADMIN, NOTE: invalid?
			var reg = installer.environmentVariable("SystemRoot") + "\\System32\\reg.exe";
			var key= "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers" ;
			component.addOperation("Execute", reg, "ADD", key, "/v", "@TargetDir@\\MaintenanceTool_Windows.exe", "/t", "REG_SZ", "/d","~ RUNASADMIN", "/f");

			// create Shortcut for Start Menu
			component.addOperation("CreateShortcut", "@TargetDir@/fastman2/mmqt2.exe", "@StartMenuDir@/秒装助手.lnk");
			component.addOperation("CreateShortcut", "@TargetDir@/MaintenanceTool_Windows.exe", "@StartMenuDir@/卸载秒装助手.lnk");
			
			// create Shortcut for Desktop
			component.addOperation("CreateShortcut", "@TargetDir@/fastman2/mmqt2.exe", "@DesktopDir@/秒装助手.lnk");			
		}
    } catch (e) {
        console.log(e);
    }
}

Component.prototype.loaded = function ()
{
    var page = gui.pageByObjectName("DynamicPage");
    if (page != null) {
        console.log("Connecting the dynamic page entered signal.");
        page.entered.connect(Component.prototype.dynamicPageEntered);
    }
}

Component.prototype.dynamicPageEntered = function ()
{
    var pageWidget = gui.pageWidgetByObjectName("DynamicPage");
    if (pageWidget != null) {
        console.log("Setting the widgets label text.")
        pageWidget.m_pageLabel.text = "This is a dynamically created page.";
    }
}

Component.prototype.installationFinishedPageIsShown = function()
{
    try {
        if (installer.isInstaller() && installer.status == QInstaller.Success) {
            installer.addWizardPageItem(component, "ReadMeCheckBoxForm", QInstaller.InstallationFinished );
        }
    } catch(e) {
        console.log(e);
    }
}

Component.prototype.installationFinished = function()
{	
    try {
        if (installer.isInstaller() && installer.status == QInstaller.Success) {			
			// README
            var isReadMeCheckBoxChecked = component.userInterface("ReadMeCheckBoxForm").readMeCheckBox.checked;
            if (isReadMeCheckBoxChecked) {
                QDesktopServices.openUrl("file:///" + installer.value("TargetDir") + "/README.txt");
            }
        }
    } catch(e) {
        console.log(e);
    }
}

// for auto install
function Controller()
{
	var widget = gui.pageById(QInstaller.Introduction); // get the introduction wizard page
    if (widget != null)
        widget.packageManagerCoreTypeChanged.connect(onPackageManagerCoreTypeChanged);
}

onPackageManagerCoreTypeChanged = function()
{
    console.log("Is Updater: " + installer.isUpdater());
    console.log("Is Uninstaller: " + installer.isUninstaller());
    console.log("Is Package Manager: " + installer.isPackageManager());
}

Controller.prototype.IntroductionPageCallback = function()
{
    var widget = gui.currentPageWidget(); // get the current wizard page
    if (widget != null) {
        widget.title = "New title."; // set the page title
        widget.MessageLabel.setText("New Message."); // set the welcome text
    }
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    gui.clickButton(buttons.NextButton); // automatically click the Next button
}
