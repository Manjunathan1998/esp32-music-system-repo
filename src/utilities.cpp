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

void printLine(String text)
{
#ifdef DEBUG_PRINT
	Serial.println(text);
#endif
}

void checkBoardMemory()
{

	Serial.println("\n=== ESP32 Memory Diagnostics ===");

	// --- PSRAM (External RAM) ---
	if (psramFound())
	{
		Serial.println("PSRAM: FOUND");

		size_t totalPSRAM = ESP.getPsramSize();
		size_t freePSRAM = ESP.getFreePsram();

		Serial.printf("  Total PSRAM: %u bytes (%.2f MB)\n", totalPSRAM, totalPSRAM / 1024.0 / 1024.0);
		Serial.printf("  Free  PSRAM: %u bytes (%.2f MB)\n", freePSRAM, freePSRAM / 1024.0 / 1024.0);
	}
	else
	{
		Serial.println("PSRAM: NOT FOUND");
	}

	// --- Internal Heap Memory ---
	Serial.println("\n--- Internal RAM (Heap) ---");
	Serial.printf("  Total Heap: %u bytes\n", ESP.getHeapSize());
	Serial.printf("  Free  Heap: %u bytes\n", ESP.getFreeHeap());
	Serial.printf("  Minimum Free Heap (lifetime): %u bytes\n", ESP.getMinFreeHeap());

	// --- Flash Memory ---
	Serial.println("\n--- Flash Memory ---");
	Serial.printf("  Flash Chip Size: %u bytes (%.2f MB)\n", ESP.getFlashChipSize(),
								ESP.getFlashChipSize() / 1024.0 / 1024.0);

	Serial.printf("  Sketch Size:     %u bytes\n", ESP.getSketchSize());
	Serial.printf("  Free Sketch Space: %u bytes\n", ESP.getFreeSketchSpace());

	Serial.println("\nDiagnostics complete.\n");
}