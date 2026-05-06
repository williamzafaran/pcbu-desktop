#include "BluetoothHelper.h"

#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#pragma comment(lib, "windowsapp")

#include <map>
#include <mutex>

using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

static BluetoothLEAdvertisementWatcher g_watcher{nullptr};
static std::mutex g_devicesMutex;
static std::map<uint64_t, BluetoothDevice> g_discoveredDevices;
static winrt::event_token g_receivedToken;

static void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher const& watcher, BluetoothLEAdvertisementReceivedEventArgs const& args) {
  std::lock_guard<std::mutex> lock(g_devicesMutex);
  char addrStr[18]{};
  BluetoothHelper::ba2str(args.BluetoothAddress(), addrStr);
  
  auto name = args.Advertisement().LocalName();
  std::string deviceName = name.empty() ? "Unknown device" : winrt::to_string(name);
  
  g_discoveredDevices[args.BluetoothAddress()] = {deviceName, std::string(addrStr)};
}

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

void BluetoothHelper::StartScan() {
  try {
    winrt::init_apartment();
  } catch(...) {}

  std::lock_guard<std::mutex> lock(g_devicesMutex);
  g_discoveredDevices.clear();

  if(g_watcher == nullptr) {
    g_watcher = BluetoothLEAdvertisementWatcher();
    g_watcher.ScanningMode(BluetoothLEScanningMode::Active);
    g_receivedToken = g_watcher.Received(OnAdvertisementReceived);
  }

  if(g_watcher.Status() != BluetoothLEAdvertisementWatcherStatus::Started) {
    g_watcher.Start();
  }
}

void BluetoothHelper::StopScan() {
  if(g_watcher != nullptr) {
    if(g_watcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started) {
      g_watcher.Stop();
    }
  }
}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
  std::lock_guard<std::mutex> lock(g_devicesMutex);
  std::vector<BluetoothDevice> result;
  for(const auto &[addr, device] : g_discoveredDevices) {
    result.push_back(device);
  }
  return result;
}

bool BluetoothHelper::PairDevice(const BluetoothDevice &device) {
  BLUETOOTH_DEVICE_INFO deviceInfo = {sizeof(deviceInfo)};
  BLUETOOTH_ADDRESS deviceAddress{};
  sscanf_s(device.address.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &deviceAddress.rgBytes[5], &deviceAddress.rgBytes[4], &deviceAddress.rgBytes[3],
           &deviceAddress.rgBytes[2], &deviceAddress.rgBytes[1], &deviceAddress.rgBytes[0]);

  deviceInfo.Address = deviceAddress;

  bool alreadyAuthenticated = false;
  DWORD getInfoRes = BluetoothGetDeviceInfo(nullptr, &deviceInfo);
  if(getInfoRes == ERROR_SUCCESS) {
    alreadyAuthenticated = deviceInfo.fAuthenticated;
  } else {
    // Device is completely new to the OS, hasn't been cached yet.
    // BluetoothAuthenticateDevice will still work.
    spdlog::info("Device not found in Windows cache. Proceeding with direct authentication.");
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
