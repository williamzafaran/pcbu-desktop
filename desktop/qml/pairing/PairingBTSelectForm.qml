import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

import PCBioUnlock
import 'qrc:/ui/base'

StepForm {
    property alias savedBTListModel: savedBTDeviceListModel
    property alias foundBTListModel: foundBTDeviceListModel
    property alias unknownBTListModel: unknownBTDeviceListModel
    property string selectedAddress: ""

    description: QI18n.Get('pairing_form_bt_select_desc')
    nextBtn.enabled: false
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Material.background
        
        ListModel { id: savedBTDeviceListModel }
        ListModel { id: foundBTDeviceListModel }
        ListModel { id: unknownBTDeviceListModel }

        Component {
            id: btDeviceDelegate
            ItemDelegate {
                Layout.fillWidth: true
                width: parent ? parent.width : 0
                height: 55
                background: Rectangle {
                    anchors.fill: parent
                    color: selectedAddress === address ? Material.accent : Material.color(Material.Grey, Material.Shade700)
                    radius: 20
                }
                contentItem: Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    spacing: 2
                    Label {
                        text: name
                        color: Material.foreground
                        font.pointSize: 11
                    }
                    Label {
                        text: address
                        color: Material.color(Material.Grey, Material.Shade400)
                        font.pointSize: 9
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        selectedAddress = address;
                        let data = PairingForm.GetData();
                        data.bluetoothAddress = address;
                        PairingForm.SetData(data);
                        nextBtn.enabled = true;
                    }
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 16
            RowLayout {
                BusyIndicator {
                    running: true
                    scale: 0.75
                }
                Label {
                    text: QI18n.Get('pairing_form_bt_scanning')
                    font.pointSize: 14
                }
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    
                    // Saved Devices
                    Label { 
                        text: "Saved Devices"
                        font.bold: true
                        color: Material.accent
                        visible: savedRepeater.count > 0 
                    }
                    Repeater { 
                        id: savedRepeater
                        model: savedBTDeviceListModel 
                        delegate: btDeviceDelegate 
                    }
                    
                    Rectangle { 
                        height: 1; Layout.fillWidth: true; 
                        color: Material.color(Material.Grey, Material.Shade800); 
                        visible: savedRepeater.count > 0 && foundRepeater.count > 0
                    }
                    
                    // Found Devices
                    Label { 
                        text: "Search Results"
                        font.bold: true
                        color: Material.accent
                        visible: foundRepeater.count > 0 
                    }
                    Repeater { 
                        id: foundRepeater
                        model: foundBTDeviceListModel 
                        delegate: btDeviceDelegate 
                    }
                    
                    Rectangle { 
                        height: 1; Layout.fillWidth: true; 
                        color: Material.color(Material.Grey, Material.Shade800); 
                        visible: (savedRepeater.count > 0 || foundRepeater.count > 0) && unknownRepeater.count > 0
                    }
                    
                    // Unknown Devices
                    Button { 
                        id: unknownBtn
                        text: "Unknown Devices (" + unknownRepeater.count + ")"
                        checkable: true
                        Layout.fillWidth: true
                        visible: unknownRepeater.count > 0
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: unknownBtn.checked && unknownRepeater.count > 0
                        Repeater { 
                            id: unknownRepeater
                            model: unknownBTDeviceListModel 
                            delegate: btDeviceDelegate 
                        }
                    }
                }
            }
        }
    }
}
