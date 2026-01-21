#ifndef BLUETOOTH_DEVICE_MANAGER_H
#define BLUETOOTH_DEVICE_MANAGER_H

#include <Arduino.h>
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"
#include "nvs.h"

#define MAX_BONDED_DEVICES 10

// Structure to hold bonded device information
struct BondedDevice
{
    uint8_t address[6];
    bool valid;
};

class BluetoothDeviceManager
{
private:
    BondedDevice devicesList[MAX_BONDED_DEVICES];
    int devicesCount;

public:
    BluetoothDeviceManager();

    // Update the list of bonded devices from NVS
    void updateBondedDevicesList();

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
