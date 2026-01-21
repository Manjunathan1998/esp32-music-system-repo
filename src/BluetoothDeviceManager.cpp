#include "BluetoothDeviceManager.h"

BluetoothDeviceManager::BluetoothDeviceManager()
{
    devicesCount = 0;
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
    }
}

void BluetoothDeviceManager::updateBondedDevicesList()
{
    // Clear existing list
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
    }

    // Get count of bonded devices
    int totalDevices = esp_bt_gap_get_bond_device_num();

    // Sanity check - if too many devices, NVS might be corrupted
    if (totalDevices > 50)
    {
        Serial.printf("ERROR: %d devices reported - NVS may be corrupted!\n", totalDevices);
        Serial.println("Consider running 'btclear' to reset bonded devices.");
        devicesCount = 0;
        return;
    }

    devicesCount = totalDevices;

    if (devicesCount > MAX_BONDED_DEVICES)
    {
        Serial.printf("Warning: %d devices bonded, but only storing first %d\n",
                      devicesCount, MAX_BONDED_DEVICES);
        devicesCount = MAX_BONDED_DEVICES;
    }

    if (devicesCount > 0)
    {
        // Allocate memory for device list
        esp_bd_addr_t *dev_list = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * devicesCount);

        if (dev_list != NULL)
        {
            // Retrieve bonded devices from NVS
            esp_err_t ret = esp_bt_gap_get_bond_device_list(&devicesCount, dev_list);

            if (ret == ESP_OK)
            {
                // Copy devices to internal list
                for (int i = 0; i < devicesCount; i++)
                {
                    memcpy(devicesList[i].address, dev_list[i], 6);
                    devicesList[i].valid = true;
                }
                Serial.printf("Successfully loaded %d bonded device(s)\n", devicesCount);
            }
            else
            {
                Serial.printf("Failed to retrieve bonded devices. Error: 0x%x\n", ret);
                devicesCount = 0;
            }

            free(dev_list);
        }
        else
        {
            Serial.println("Failed to allocate memory for bonded devices list");
            devicesCount = 0;
        }
    }
    else
    {
        Serial.println("No bonded devices found in NVS");
    }
}

int BluetoothDeviceManager::getBondedDevicesCount()
{
    return devicesCount;
}

String BluetoothDeviceManager::getMacAddressString(int index)
{
    if (index >= 0 && index < devicesCount && devicesList[index].valid)
    {
        char macStr[18];
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                devicesList[index].address[0],
                devicesList[index].address[1],
                devicesList[index].address[2],
                devicesList[index].address[3],
                devicesList[index].address[4],
                devicesList[index].address[5]);
        return String(macStr);
    }
    return "N/A";
}

bool BluetoothDeviceManager::getMacAddressBytes(int index, uint8_t *macBytes)
{
    if (index >= 0 && index < devicesCount && devicesList[index].valid && macBytes != NULL)
    {
        memcpy(macBytes, devicesList[index].address, 6);
        return true;
    }
    return false;
}

bool BluetoothDeviceManager::removeBondedDevice(int index)
{
    if (index >= 0 && index < devicesCount && devicesList[index].valid)
    {
        esp_err_t ret = esp_bt_gap_remove_bond_device(devicesList[index].address);
        if (ret == ESP_OK)
        {
            Serial.printf("Device %d unbonded successfully\n", index);
            updateBondedDevicesList(); // Refresh list after removal
            return true;
        }
        else
        {
            Serial.printf("Failed to unbond device %d. Error: 0x%x\n", index, ret);
        }
    }
    else
    {
        Serial.printf("Invalid device index: %d\n", index);
    }
    return false;
}

bool BluetoothDeviceManager::removeBondedDeviceByMac(uint8_t *macAddress)
{
    if (macAddress == NULL)
    {
        return false;
    }

    esp_err_t ret = esp_bt_gap_remove_bond_device(macAddress);
    if (ret == ESP_OK)
    {
        Serial.println("Device unbonded successfully by MAC");
        updateBondedDevicesList(); // Refresh list after removal
        return true;
    }
    else
    {
        Serial.printf("Failed to unbond device by MAC. Error: 0x%x\n", ret);
    }
    return false;
}

void BluetoothDeviceManager::clearAllBondedDevices()
{
    Serial.println("Clearing all bonded devices...");

    // Get raw count first
    int totalDevices = esp_bt_gap_get_bond_device_num();

    if (totalDevices == 0)
    {
        Serial.println("No bonded devices to clear");
        devicesCount = 0;
        return;
    }

    Serial.printf("Attempting to clear %d device(s)...\\n", totalDevices);

    // For corrupted NVS, try to get and clear whatever we can
    int maxToClear = (totalDevices > 100) ? 100 : totalDevices;
    esp_bd_addr_t *dev_list = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * maxToClear);

    int clearedCount = 0;
    if (dev_list != NULL)
    {
        int deviceCount = maxToClear;
        esp_err_t ret = esp_bt_gap_get_bond_device_list(&deviceCount, dev_list);

        if (ret == ESP_OK)
        {
            // Clear each device
            for (int i = 0; i < deviceCount; i++)
            {
                ret = esp_bt_gap_remove_bond_device(dev_list[i]);
                if (ret == ESP_OK)
                {
                    clearedCount++;
                }
            }
        }
        else
        {
            Serial.printf("Failed to get device list. Error: 0x%x\\n", ret);
        }

        free(dev_list);
    }
    else
    {
        Serial.println("Memory allocation failed");
    }

    Serial.printf("Cleared %d bonded device(s)\\n", clearedCount);

    // Update internal list
    updateBondedDevicesList();
}

bool BluetoothDeviceManager::eraseBluetoothNVS()
{
    Serial.println("WARNING: Erasing Bluetooth NVS namespace...");
    Serial.println("This will remove all Bluetooth pairing data!");

    // Erase the bt_nvs namespace
    esp_err_t err = nvs_flash_erase_partition("nvs");

    if (err == ESP_OK)
    {
        Serial.println("NVS erased successfully");
        // Reinitialize NVS
        err = nvs_flash_init();
        if (err == ESP_OK)
        {
            Serial.println("NVS reinitialized successfully");
            devicesCount = 0;
            for (int i = 0; i < MAX_BONDED_DEVICES; i++)
            {
                devicesList[i].valid = false;
            }
            return true;
        }
        else
        {
            Serial.printf("NVS reinit failed: 0x%x\n", err);
            return false;
        }
    }
    else
    {
        Serial.printf("NVS erase failed: 0x%x\n", err);
        return false;
    }
}

void BluetoothDeviceManager::printBondedDevices()
{
    Serial.println("========================================");
    Serial.println("    BONDED BLUETOOTH DEVICES");
    Serial.println("========================================");

    if (devicesCount == 0)
    {
        Serial.println("  No bonded devices found");
    }
    else
    {
        Serial.printf("  Total: %d device(s)\n\n", devicesCount);
        for (int i = 0; i < devicesCount; i++)
        {
            if (devicesList[i].valid)
            {
                Serial.printf("  [%d] %s\n", i + 1, getMacAddressString(i).c_str());
            }
        }
    }

    Serial.println("========================================");
}

bool BluetoothDeviceManager::isDeviceBonded(uint8_t *macAddress)
{
    if (macAddress == NULL)
    {
        return false;
    }

    for (int i = 0; i < devicesCount; i++)
    {
        if (devicesList[i].valid)
        {
            if (memcmp(devicesList[i].address, macAddress, 6) == 0)
            {
                return true;
            }
        }
    }
    return false;
}
