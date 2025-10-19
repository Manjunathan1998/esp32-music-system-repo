#include "utilities.h"

void listSPIFFS()
{
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    Serial.println("SPIFFS file list:");
    while (file)
    {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}