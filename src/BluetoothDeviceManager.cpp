#include "BluetoothDeviceManager.h"
#include "esp_bt_main.h" // For esp_bluedroid_get_status()

BluetoothDeviceManager::BluetoothDeviceManager()
{
    devicesCount = 0;
    pendingDevice.pending = false;
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
        devicesList[i].name[0] = '\0';
    }
}

void BluetoothDeviceManager::queueDeviceForSave(uint8_t *macAddress, const char *deviceName)
{
    if (macAddress == NULL)
        return;

    memcpy(pendingDevice.address, macAddress, 6);

    if (deviceName != NULL && strlen(deviceName) > 0)
    {
        strncpy(pendingDevice.name, deviceName, MAX_DEVICE_NAME_LEN - 1);
        pendingDevice.name[MAX_DEVICE_NAME_LEN - 1] = '\0';
    }
    else
    {
        snprintf(pendingDevice.name, sizeof(pendingDevice.name), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAddress[0], macAddress[1], macAddress[2],
                 macAddress[3], macAddress[4], macAddress[5]);
    }

    pendingDevice.pending = true;
    Serial.printf("[BT History] Queued device for save: %s\n", pendingDevice.name);
}

void BluetoothDeviceManager::processPendingSaves()
{
    if (pendingDevice.pending)
    {
        Serial.println("[BT History] Processing pending device save...");
        addDevice(pendingDevice.address, pendingDevice.name);
        pendingDevice.pending = false;
    }
}

void BluetoothDeviceManager::loadDevicesFromFile()
{
    // Clear existing list
    devicesCount = 0;
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
        devicesList[i].name[0] = '\0';
    }

    if (!SPIFFS.exists(BT_DEVICES_FILE))
    {
        Serial.println("[BT Load] No device file found");
        return;
    }

    File file = SPIFFS.open(BT_DEVICES_FILE, "r");
    if (!file)
    {
        Serial.println("[BT Load] Failed to open file");
        return;
    }

    Serial.println("[BT Load] Loading devices from SPIFFS...");
    int count = 0;

    Serial.printf("[BT Load] File contents:\n");
    while (file.available() && count < MAX_BONDED_DEVICES)
    {
        String line = file.readStringUntil('\n');
        line.trim();

        Serial.printf("[BT Load]   Raw line %d: '%s'\n", count, line.c_str());

        if (line.length() == 0)
        {
            Serial.println("[BT Load]   -> Empty line, skipping");
            continue;
        }

        int separatorIndex = line.indexOf('|');
        if (separatorIndex == -1)
        {
            Serial.println("[BT Load]   -> No separator found, skipping");
            continue;
        }

        String macStr = line.substring(0, separatorIndex);
        String nameStr = line.substring(separatorIndex + 1);

        Serial.printf("[BT Load]   -> MAC: '%s', Name: '%s'\n", macStr.c_str(), nameStr.c_str());

        if (macStr.length() == 17)
        {
            uint8_t mac[6];
            if (sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6)
            {
                memcpy(devicesList[count].address, mac, 6);
                strncpy(devicesList[count].name, nameStr.c_str(), MAX_DEVICE_NAME_LEN - 1);
                devicesList[count].name[MAX_DEVICE_NAME_LEN - 1] = '\0';
                devicesList[count].valid = true;
                Serial.printf("[BT Load] [%d] %s - %s\n", count, macStr.c_str(), nameStr.c_str());
                count++;
            }
        }
    }

    file.close();
    devicesCount = count;
    Serial.printf("[BT Load] Loaded %d device(s) from file\n", devicesCount);
}

int BluetoothDeviceManager::findDeviceByMac(uint8_t *macAddress)
{
    if (macAddress == NULL)
        return -1;

    for (int i = 0; i < devicesCount; i++)
    {
        if (devicesList[i].valid &&
            memcmp(devicesList[i].address, macAddress, 6) == 0)
        {
            return i;
        }
    }
    return -1;
}

String BluetoothDeviceManager::lookupDeviceNameInFile(uint8_t *macAddress)
{
    if (macAddress == NULL || !SPIFFS.exists(BT_DEVICES_FILE))
    {
        Serial.println("[BT Lookup] File doesn't exist or NULL MAC");
        return "";
    }

    File file = SPIFFS.open(BT_DEVICES_FILE, "r");
    if (!file)
    {
        Serial.println("[BT Lookup] Failed to open file");
        return "";
    }

    char searchMac[18];
    snprintf(searchMac, sizeof(searchMac), "%02X:%02X:%02X:%02X:%02X:%02X",
             macAddress[0], macAddress[1], macAddress[2],
             macAddress[3], macAddress[4], macAddress[5]);
    Serial.printf("[BT Lookup] Searching for: %s\n", searchMac);

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() == 0)
            continue;

        int pipeIndex = line.indexOf('|');
        if (pipeIndex < 0)
            continue;

        String macStr = line.substring(0, pipeIndex);
        String nameStr = line.substring(pipeIndex + 1);

        Serial.printf("[BT Lookup] Checking line: %s\n", line.c_str());

        if (macStr.length() == 17)
        {
            uint8_t mac[6];
            if (sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6)
            {
                if (memcmp(mac, macAddress, 6) == 0)
                {
                    file.close();
                    Serial.printf("[BT Lookup] FOUND: %s\n", nameStr.c_str());
                    return nameStr;
                }
            }
        }
    }

    file.close();
    Serial.println("[BT Lookup] Not found");
    return "";
}

void BluetoothDeviceManager::saveDevicesToFile()
{
    File file = SPIFFS.open(BT_DEVICES_FILE, "w");
    if (!file)
    {
        Serial.println("[BT History] ERROR: Failed to open file for writing");
        return;
    }

    for (int i = 0; i < devicesCount; i++)
    {
        if (devicesList[i].valid && strlen(devicesList[i].name) > 0)
        {
            file.printf("%02X:%02X:%02X:%02X:%02X:%02X|%s\n",
                        devicesList[i].address[0], devicesList[i].address[1],
                        devicesList[i].address[2], devicesList[i].address[3],
                        devicesList[i].address[4], devicesList[i].address[5],
                        devicesList[i].name);
        }
    }

    file.close();
    Serial.println("[BT History] File saved successfully");
}

void BluetoothDeviceManager::addDevice(uint8_t *macAddress, const char *deviceName)
{
    if (macAddress == NULL)
        return;

    char defaultName[18];
    if (deviceName == NULL || strlen(deviceName) == 0)
    {
        snprintf(defaultName, sizeof(defaultName), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAddress[0], macAddress[1], macAddress[2],
                 macAddress[3], macAddress[4], macAddress[5]);
        deviceName = defaultName;
    }

    Serial.printf("[BT History] Adding device: %s\n", deviceName);

    // Load existing names from file
    if (!SPIFFS.exists(BT_DEVICES_FILE))
    {
        File file = SPIFFS.open(BT_DEVICES_FILE, "w");
        if (file)
            file.close();
    }

    // Read current file into temporary storage
    String lines[MAX_BONDED_DEVICES];
    int lineCount = 0;
    bool deviceFound = false;

    File file = SPIFFS.open(BT_DEVICES_FILE, "r");
    if (file)
    {
        while (file.available() && lineCount < MAX_BONDED_DEVICES)
        {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
                continue;

            int pipeIndex = line.indexOf('|');
            if (pipeIndex < 0)
                continue;

            String macStr = line.substring(0, pipeIndex);
            uint8_t mac[6];

            if (macStr.length() == 17 &&
                sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6)
            {
                if (memcmp(mac, macAddress, 6) == 0)
                {
                    // Update this device's name
                    char macBuf[18];
                    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                             macAddress[0], macAddress[1], macAddress[2],
                             macAddress[3], macAddress[4], macAddress[5]);
                    lines[lineCount++] = String(macBuf) + "|" + String(deviceName);
                    deviceFound = true;
                }
                else
                {
                    lines[lineCount++] = line;
                }
            }
        }
        file.close();
    }

    // Add new device if not found
    if (!deviceFound && lineCount < MAX_BONDED_DEVICES)
    {
        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAddress[0], macAddress[1], macAddress[2],
                 macAddress[3], macAddress[4], macAddress[5]);
        lines[lineCount++] = String(macBuf) + "|" + String(deviceName);
    }
    else if (!deviceFound && lineCount >= MAX_BONDED_DEVICES)
    {
        // Remove oldest, add new
        for (int i = 0; i < MAX_BONDED_DEVICES - 1; i++)
        {
            lines[i] = lines[i + 1];
        }
        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAddress[0], macAddress[1], macAddress[2],
                 macAddress[3], macAddress[4], macAddress[5]);
        lines[MAX_BONDED_DEVICES - 1] = String(macBuf) + "|" + String(deviceName);
        lineCount = MAX_BONDED_DEVICES;
    }

    // Write back to file
    file = SPIFFS.open(BT_DEVICES_FILE, "w");
    if (file)
    {
        Serial.printf("[BT History] Writing %d lines to file:\n", lineCount);
        for (int i = 0; i < lineCount; i++)
        {
            file.println(lines[i]);
            Serial.printf("[BT History]   Line %d: %s\n", i, lines[i].c_str());
        }
        file.close();
        Serial.println("[BT History] Device saved to file successfully");
    }
    else
    {
        Serial.println("[BT History] ERROR: Failed to open file for writing!");
    }
}

String BluetoothDeviceManager::getDeviceName(int index)
{
    if (index >= 0 && index < devicesCount && devicesList[index].valid)
    {
        if (strlen(devicesList[index].name) > 0)
        {
            return String(devicesList[index].name);
        }
        return getMacAddressString(index);
    }
    return "N/A";
}

void BluetoothDeviceManager::updateBondedDevicesList()
{
    // Clear existing list
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
    }

    // Safety check: Verify Bluetooth is initialized before calling GAP functions
    // If BT is not initialized, esp_bt_gap_get_bond_device_num() returns garbage data
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    {
        Serial.println("WARNING: Bluetooth not initialized - cannot read bonded devices");
        Serial.println("Switch to BT mode first to initialize Bluetooth stack");
        devicesCount = 0;
        return;
    }

    // Get count of bonded devices
    int totalDevices = esp_bt_gap_get_bond_device_num();

    // Sanity check - if too many devices, NVS might be corrupted
    // if (totalDevices > 50)
    // {
    //     Serial.printf("ERROR: %d devices reported - NVS may be corrupted!\n", totalDevices);
    //     Serial.println("Consider running 'btclear' to reset bonded devices.");
    //     devicesCount = 0;
    //     return;
    // }

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
                // Copy devices to internal list and lookup names from SPIFFS
                Serial.println("[BT] Looking up device names from SPIFFS...");
                for (int i = 0; i < devicesCount; i++)
                {
                    memcpy(devicesList[i].address, dev_list[i], 6);
                    devicesList[i].valid = true;

                    char macStr[18];
                    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                             dev_list[i][0], dev_list[i][1], dev_list[i][2],
                             dev_list[i][3], dev_list[i][4], dev_list[i][5]);

                    // Try to get name from SPIFFS file
                    String storedName = lookupDeviceNameInFile(dev_list[i]);
                    if (storedName.length() > 0)
                    {
                        strncpy(devicesList[i].name, storedName.c_str(), MAX_DEVICE_NAME_LEN - 1);
                        devicesList[i].name[MAX_DEVICE_NAME_LEN - 1] = '\0';
                        Serial.printf("[BT] Device %d (%s): Found name '%s'\n", i, macStr, devicesList[i].name);
                    }
                    else
                    {
                        devicesList[i].name[0] = '\0';
                        Serial.printf("[BT] Device %d (%s): No name found, will show MAC\n", i, macStr);
                    }
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

bool BluetoothDeviceManager::clearAllDevices()
{
    Serial.println("[BT Clear] Clearing all devices...");

    // Step 1: Clear SPIFFS file
    if (SPIFFS.exists(BT_DEVICES_FILE))
    {
        if (SPIFFS.remove(BT_DEVICES_FILE))
        {
            Serial.println("[BT Clear] SPIFFS file deleted");
        }
        else
        {
            Serial.println("[BT Clear] ERROR: Failed to delete SPIFFS file");
        }
    }
    else
    {
        Serial.println("[BT Clear] SPIFFS file doesn't exist");
    }

    // Step 2: Clear internal list
    devicesCount = 0;
    for (int i = 0; i < MAX_BONDED_DEVICES; i++)
    {
        devicesList[i].valid = false;
        devicesList[i].name[0] = '\0';
    }

    // Step 3: Clear NVS bonded devices (if BT is initialized)
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
    {
        Serial.println("[BT Clear] Clearing bonded devices from NVS...");

        int totalDevices = esp_bt_gap_get_bond_device_num();
        Serial.printf("[BT Clear] Found %d bonded device(s) in NVS\n", totalDevices);

        if (totalDevices > 0)
        {
            int maxToClear = (totalDevices > 100) ? 100 : totalDevices;
            esp_bd_addr_t *dev_list = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * maxToClear);

            if (dev_list != NULL)
            {
                int deviceCount = maxToClear;
                esp_err_t ret = esp_bt_gap_get_bond_device_list(&deviceCount, dev_list);

                if (ret == ESP_OK)
                {
                    int clearedCount = 0;
                    for (int i = 0; i < deviceCount; i++)
                    {
                        ret = esp_bt_gap_remove_bond_device(dev_list[i]);
                        if (ret == ESP_OK)
                        {
                            clearedCount++;
                        }
                    }
                    Serial.printf("[BT Clear] Cleared %d bonded device(s) from NVS\n", clearedCount);
                }
                else
                {
                    Serial.printf("[BT Clear] ERROR: Failed to get NVS device list: 0x%x\n", ret);
                }

                free(dev_list);
            }
            else
            {
                Serial.println("[BT Clear] ERROR: Memory allocation failed");
            }
        }
        else
        {
            Serial.println("[BT Clear] No bonded devices in NVS to clear");
        }
    }
    else
    {
        Serial.println("[BT Clear] WARNING: Bluetooth not initialized - NVS bonding data not cleared");
        Serial.println("[BT Clear] Note: Only SPIFFS file was cleared");
    }

    Serial.println("[BT Clear] Clear operation completed");
    return true;
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

bool BluetoothDeviceManager::isNvsCorrupted()
{
    // Safety check: Only check if Bluetooth is initialized
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    {
        Serial.println("[NVS Check] Bluetooth not initialized - skipping corruption check");
        return false; // Can't determine corruption if BT not initialized
    }

    // Check raw device count from NVS without updating internal state
    int totalDevices = esp_bt_gap_get_bond_device_num();
    return (totalDevices > 50);
}

bool BluetoothDeviceManager::eraseBluetoothNVS()
{
    Serial.println("WARNING: Erasing Bluetooth NVS namespace...");
    Serial.println("This will remove all Bluetooth pairing data!");

    // Open NVS handle to bt_nvs namespace
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("bt_nvs", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK)
    {
        Serial.printf("Failed to open bt_nvs namespace: 0x%x\n", err);
        Serial.println("ALTERNATIVE: Erasing entire NVS partition (last resort)...");
        err = nvs_flash_erase_partition("nvs");
        if (err == ESP_OK)
        {
            Serial.println("NVS partition erased - reinitializing...");
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
        }
        Serial.printf("Complete NVS erase failed: 0x%x\n", err);
        return false;
    }

    // Erase all keys in the bt_nvs namespace
    err = nvs_erase_all(nvs_handle);
    if (err == ESP_OK)
    {
        // Commit the erase operation
        err = nvs_commit(nvs_handle);
        nvs_close(nvs_handle);

        if (err == ESP_OK)
        {
            Serial.println("Bluetooth NVS namespace erased successfully");
            devicesCount = 0;
            for (int i = 0; i < MAX_BONDED_DEVICES; i++)
            {
                devicesList[i].valid = false;
            }
            return true;
        }
        else
        {
            Serial.printf("NVS commit failed: 0x%x\n", err);
            return false;
        }
    }
    else
    {
        nvs_close(nvs_handle);
        Serial.printf("Bluetooth NVS erase failed: 0x%x\n", err);
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
