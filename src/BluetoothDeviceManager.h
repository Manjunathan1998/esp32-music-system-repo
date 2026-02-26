#ifndef BLUETOOTH_DEVICE_MANAGER_H
#define BLUETOOTH_DEVICE_MANAGER_H

#include <Arduino.h>
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "SPIFFS.h"

#define MAX_BONDED_DEVICES 5
#define MAX_DEVICE_NAME_LEN 64
#define BT_DEVICES_FILE "/bt_devices.txt"

// Structure to hold bonded device information
struct BondedDevice
{
    uint8_t address[6];
    char name[MAX_DEVICE_NAME_LEN];
    bool valid;
};

struct PendingDevice
{
    uint8_t address[6];
    char name[MAX_DEVICE_NAME_LEN];
    bool pending;
};

class BluetoothDeviceManager
{
private:
    BondedDevice devicesList[MAX_BONDED_DEVICES];
    int devicesCount;
    PendingDevice pendingDevice;

    // Private helper methods
    int findDeviceByMac(uint8_t *macAddress);
    void saveDevicesToFile();
    String lookupDeviceNameInFile(uint8_t *macAddress);

public:
    BluetoothDeviceManager();

    // Queue a device to be saved (safe to call from interrupt)
    void queueDeviceForSave(uint8_t *macAddress, const char *deviceName);

    // Process pending device saves (call from main loop)
    void processPendingSaves();

    // Add or update a device in the history (NOT interrupt-safe)
    void addDevice(uint8_t *macAddress, const char *deviceName);

    // Get device name by index (returns name or MAC if no name)
    String getDeviceName(int index);

    // Update the list of bonded devices from NVS and populate names from SPIFFS
    void updateBondedDevicesList();

    // Load devices from SPIFFS file (recent connections)
    void loadDevicesFromFile();

    // Clear all devices (SPIFFS file and NVS bonded devices)
    bool clearAllDevices();

    // Get the number of bonded devices
    int getBondedDevicesCount();

    // Get MAC address as formatted string (XX:XX:XX:XX:XX:XX)
    String getMacAddressString(int index);

    // Get raw MAC address bytes
    bool getMacAddressBytes(int index, uint8_t *macBytes);

    // Remove a specific bonded device by index
    bool removeBondedDevice(int index);

    // Remove a specific bonded device by MAC address
    bool removeBondedDeviceByMac(uint8_t *macAddress);

    // Clear all bonded devices
    void clearAllBondedDevices();

    // Force erase Bluetooth NVS namespace (for corrupted data)
    bool eraseBluetoothNVS();

    // Check if NVS is corrupted (without modifying internal state)
    bool isNvsCorrupted();

    // Print bonded devices to Serial
    void printBondedDevices();

    // Check if a device is in the bonded list
    bool isDeviceBonded(uint8_t *macAddress);
};

#endif // BLUETOOTH_DEVICE_MANAGER_H
