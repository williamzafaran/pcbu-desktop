import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import PCBioUnlock

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    minimumWidth: 1024
    minimumHeight: 768
    visible: true
    title: 'PC Bio Unlock'

    property bool canClose: true
    onClosing: function(close) { close.accepted = window.canClose }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25
        Label {
            id: title
            text: 'PC Bio Unlock'
            font.pointSize: 36
        }
        Loader {
            id: viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: "qrc:/ui/forms/MainForm.qml"
        }
    }

    property var onMessageDialogAccept: function () {};
    property var onMessageDialogConfirmAccept: function () {};
    MessageDialog {
        id: messageDialog
        title: 'Title'
        text: 'Text'
        onAccepted: onMessageDialogAccept()
        onRejected: onMessageDialogAccept()
    }
    MessageDialog {
        id: confirmMessageDialog
        title: QI18n.Get('confirm');
        text: 'Text'
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: onMessageDialogConfirmAccept()
    }

    function showFatalErrorMessage(text) {
        messageDialog.title = QI18n.Get('error');
        messageDialog.text = text;
        messageDialog.visible = true;
        onMessageDialogAccept = function() { Qt.exit(1); }
    }
    function showErrorMessage(text) {
        messageDialog.title = QI18n.Get('error');
        messageDialog.text = text;
        messageDialog.visible = true;
    }
    function showConfirmMessage(text, onAccept) {
        confirmMessageDialog.text = text;
        confirmMessageDialog.visible = true;
        onMessageDialogConfirmAccept = onAccept;
    }

    function showLoadingScreen(text) {
        viewLoader.source = "qrc:/ui/forms/LoadingForm.qml";
        viewLoader.item.okBtn.enabled = false;
        viewLoader.item.progressLbl.text = text;
        window.canClose = false;
    }
    function appendLoadingOutput(text) {
        viewLoader.item.outputTxtArea.text += text + '\n';
    }
    function finishLoadingScreen(text) {
        window.canClose = true;
        viewLoader.item.okBtn.enabled = true;
        viewLoader.item.progressBar.indeterminate = false;
        viewLoader.item.progressBar.value = 100;
        viewLoader.item.progressLbl.text = text;
    }

    function updateBluetoothDeviceList(devices) {
        let selAddress = undefined;
        try {
            selAddress = viewLoader.item.selectedAddress;
        } catch (e) {}
        
        viewLoader.item.savedBTListModel.clear();
        viewLoader.item.foundBTListModel.clear();
        viewLoader.item.unknownBTListModel.clear();
        
        for(let i = 0; i < devices.length; i++) {
            let cat = devices[i].category;
            let entry = {name: devices[i].name, address: devices[i].address};
            if(cat === 0) viewLoader.item.savedBTListModel.append(entry);
            else if(cat === 1) viewLoader.item.foundBTListModel.append(entry);
            else viewLoader.item.unknownBTListModel.append(entry);
        }
    }
    function finishBluetoothPairing(isSuccess) {
        if(!isSuccess) {
            showErrorMessage(QI18n.Get('error_bluetooth_pairing'));
            PairingForm.OnBackClicked(viewLoader, window);
            return;
        }
        PairingForm.OnNextClicked(viewLoader, window);
    }

    function showUpdaterWindow() {
        let updaterWin = Qt.createComponent("qrc:/ui/UpdaterWindow.qml").createObject(window);
        updaterWin.show();
    }

    Component.onCompleted: {
        if(MainWindow.PerformStartupChecks(viewLoader, window)) {
            UpdaterWindow.CheckForUpdates(window);
        }
    }
}
