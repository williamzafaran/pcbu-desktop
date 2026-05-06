#include "BluetoothHelper.h"

#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

bool BluetoothHelper::IsAvailable() {
  BLUETOOTH_FIND_RADIO_PARAMS params{};
  params.dwSize = sizeof(params);
  HANDLE hRadio = nullptr;
  HANDLE result = BluetoothFindFirstRadio(&params, &hRadio);
  if(result) {
    CloseHandle(hRadio);
    BluetoothFindRadioClose(result);
    return true;
  }
  return false;
}

void BluetoothHelper::StartScan() {}

void BluetoothHelper::StopScan() {}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
  // fIssueInquiry=TRUE is necessary here because this is triggered by the user
  // manually clicking "Scan" in the UI to pair a NEW device. New devices will
  // only be discovered if the radio actively scans while the phone is discoverable.
  BLUETOOTH_DEVICE_SEARCH_PARAMS params{};
  params.dwSize = sizeof(params);
  params.fReturnAuthenticated = TRUE; // paired + authenticated devices
  params.fReturnRemembered    = TRUE; // remembered (ever seen) devices
  params.fReturnUnknown       = TRUE; // devices seen but not yet paired
  params.fReturnConnected     = TRUE; // currently connected devices
  params.fIssueInquiry        = TRUE; // Trigger a physical radio scan for new devices
  params.cTimeoutMultiplier   = 4;    // ~5.12 seconds scan duration
  params.hRadio               = nullptr; // search all radios

  BLUETOOTH_DEVICE_INFO deviceInfo{};
  deviceInfo.dwSize = sizeof(deviceInfo);

  HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&params, &deviceInfo);
  if(!hFind) {
    DWORD err = GetLastError();
    if(err != ERROR_NO_MORE_ITEMS)
      spdlog::error("BluetoothFindFirstDevice() failed. (Code={})", err);
    return {};
  }

  auto devices = std::vector<BluetoothDevice>();
  do {
    char addr[18]{};
    ba2str(deviceInfo.Address.ullLong, addr);
    auto name = StringUtils::FromWideString(deviceInfo.szName);
    if(name.empty())
      name = "Unknown device";
    devices.push_back({name, std::string(addr)});
  } while(BluetoothFindNextDevice(hFind, &deviceInfo));

  BluetoothFindDeviceClose(hFind);
  return devices;
}

bool BluetoothHelper::PairDevice(const BluetoothDevice &device) {
  BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(deviceInfo)};
  BLUETOOTH_ADDRESS deviceAddress{};
  sscanf_s(device.address.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &deviceAddress.rgBytes[5], &deviceAddress.rgBytes[4], &deviceAddress.rgBytes[3],
           &deviceAddress.rgBytes[2], &deviceAddress.rgBytes[1], &deviceAddress.rgBytes[0]);

  // fIssueInquiry=FALSE: avoid radio scan (prevents error 10108 when other BT
  // devices are connected). fReturnRemembered=TRUE: find phones that are known
  // but not currently discoverable (screen off, background mode).
  BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {sizeof(searchParams),
    TRUE,  // fReturnAuthenticated
    TRUE,  // fReturnRemembered  ← was FALSE, missed screen-off phones
    TRUE,  // fReturnUnknown
    TRUE,  // fReturnConnected
    FALSE, // fIssueInquiry     ← was TRUE, caused radio contention
    0,
    nullptr};
  HBLUETOOTH_DEVICE_FIND searchHandle = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
  if(!searchHandle) {
    spdlog::error("Error getting bluetooth search handle. (Code={})", GetLastError());
    return false;
  }
  bool deviceFound = false;
  bool alreadyAuthenticated = false;
  do {
    char devAddr[18]{};
    char targetAddr[18]{};
    ba2str(deviceInfo.Address.ullLong, devAddr);
    ba2str(deviceAddress.ullLong, targetAddr);
    if(strcmp(devAddr, targetAddr) == 0) {
      deviceFound = true;
      alreadyAuthenticated = deviceInfo.fAuthenticated;
      break;
    }
  } while(BluetoothFindNextDevice(searchHandle, &deviceInfo));
  BluetoothFindDeviceClose(searchHandle);
  if(!deviceFound) {
    spdlog::error("Bluetooth device not found.");
    return false;
  }

  // If already paired at the OS level, skip authentication — calling
  // BluetoothAuthenticateDevice on an already-authenticated device returns
  // an error and was the direct cause of the "pairing failed" dialog.
  if(alreadyAuthenticated) {
    spdlog::info("Bluetooth device is already authenticated, skipping OS pairing.");
    return true;
  }

  HWND hwnd = FindWindowA(nullptr, "PC Bio Unlock");
  DWORD result = BluetoothAuthenticateDevice(hwnd, nullptr, &deviceInfo, nullptr, 0);
  if(result == ERROR_SUCCESS || result == ERROR_NO_MORE_ITEMS)
    return true;
  spdlog::error("Error while bluetooth pairing. (Code={})", result);
  return false;
}

int BluetoothHelper::str2ba(const char *straddr, BTH_ADDR *btaddr) {
  unsigned int aaddr[6]{};
  if(sscanf_s(straddr, "%02x:%02x:%02x:%02x:%02x:%02x", &aaddr[0], &aaddr[1], &aaddr[2], &aaddr[3], &aaddr[4], &aaddr[5]) != 6)
    return 1;
  *btaddr = 0;
  for(unsigned int i : aaddr) {
    auto tmpaddr = static_cast<BTH_ADDR>(i & 0xff);
    *btaddr = ((*btaddr) << 8) + tmpaddr;
  }
  return 0;
}

int BluetoothHelper::ba2str(const BTH_ADDR btaddr, char *straddr) {
  unsigned char bytes[6]{};
  for(int i = 0; i < 6; i++)
    bytes[5 - i] = static_cast<unsigned char>((btaddr >> (i * 8)) & 0xff);
  if(sprintf_s(straddr, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]) != 17)
    return 1;
  return 0;
}

std::optional<SDPService> BluetoothHelper::RegisterSDPService(SOCKADDR address) {
  GUID guid = {0x62182bf7, 0x97c8, 0x45f9, {0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xe0}};
  auto service = new WSAQUERYSETW;
  std::memset(service, 0, sizeof(WSAQUERYSETW));
  service->dwSize = sizeof(WSAQUERYSETW);
  service->lpszServiceInstanceName = (LPWSTR)L"PC Bio Unlock BT";
  service->lpServiceClassId = &guid;
  service->dwNumberOfCsAddrs = 1;
  CSADDR_INFO csAddr{};
  csAddr.LocalAddr.iSockaddrLength = sizeof(address);
  csAddr.LocalAddr.lpSockaddr = (LPSOCKADDR)&address;
  csAddr.iSocketType = SOCK_STREAM;
  csAddr.iProtocol = BTHPROTO_RFCOMM;
  service->lpcsaBuffer = &csAddr;
  if(WSASetServiceW(service, RNRSERVICE_REGISTER, 0) != 0)
    return {};
  SDPService sdpService{};
  sdpService.handle = service;
  return sdpService;
}

bool BluetoothHelper::CloseSDPService(SDPService &service) {
  if(service.handle == nullptr)
    return false;
  if(WSASetServiceW((LPWSAQUERYSETW)service.handle, RNRSERVICE_DELETE, 0) != 0)
    return false;
  service.handle = nullptr;
  return true;
}
