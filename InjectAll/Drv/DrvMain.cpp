// Main Driver entry cpp file

#include "CFunc.h"
#include "CSection.h"

// Global variables

extern "C" {
	PDRIVER_OBJECT g_DriveObject; // Driver Object - read Only (for reference counting)
}

IMAGE_LOAD_FLAGS g_flags; // Global notification flags


CSection sec;                                   //Native section object

#ifdef _WIN64
CSection secWow;                                //WOW64 section object (used only for a 64-bit build)
#endif



void OnLoadImage(
	PUNICODE_STRING FullImageName,	// The name of the image being loaded
	HANDLE ProcessId,				// The process into which the image is being loaded
	PIMAGE_INFO ImageInfo			// Containing information about the loaded image, such as its base address and size.
) 
{
	// Called back notification that an image is loaded (or mapped in memory)
	// ProcessId = process where the image is mapped into (or 0 for a driver)
	UNREFERENCED_PARAMETER(FullImageName);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(ImageInfo);

	NTSTATUS status;

	// NTSTATUS status;

	ASSERT(FullImageName);		//  Ensure that these pointers are not null
	ASSERT(ImageInfo);			// ensure that these pointers are not null

	STATIC_UNICODE_STRING(kernel32, "\\kernel32.dll");

	// We are looking for kernel32.dll only - skip the rest
	if (!ImageInfo->SystemModeImage &&			// Skip anything mapped into kernel
		ProcessId == PsGetCurrentProcessId() &&	// Our section can be mapped remotely into this process - we don't need that
		CFunc::IsSuffixedUnicodeString(FullImageName, &kernel32) &&
		CFunc::IsMappedByLdrLoadDll(&kernel32)
#if defined(_DEBUG) && defined(LIMIT_INJECTION_TO_PROC)		
		&& CFunc::IsSpecificProcessW(ProcessId, LIMIT_INJECTION_TO_PROC, FALSE)  //For debug build limit it to specific process only (for testing purposes)
#endif
		)
	{
#ifdef _WIN64
		//Is it a 32-bit process running in a 64-bit OS
		BOOLEAN bWowProc = IoIs32bitProcess(NULL);
#else
		//Cannot be a WOW64 process on a 32-bit OS
		BOOLEAN bWowProc = FALSE;
		UNREFERENCED_PARAMETER(bWowProc);
#endif
		//Now we can proceed with our injection
		DbgPrintLine("Image load (WOW=%d) for PID=%u: \"%wZ\"", bWowProc, (ULONG)(ULONG_PTR)ProcessId, FullImageName);
		
		//Get our (DLL) section to inject
		DLL_STATS* pDS;
		status = sec.GetSection(&pDS);
		if (NT_SUCCESS(status))
		{
			
		}
		else
		{
			//Error
			DbgPrintLine("ERROR: (0x%X) sec.GetSection, PID=%u", status, (ULONG)(ULONG_PTR)ProcessId);
		}

		//The following only applies to a 64-bit build
		//INFO: We need to inject our DLL into a 32-bit process too...
#ifdef _WIN64
		if (bWowProc)
		{
			status = secWow.GetSection(&pDS);
			if (NT_SUCCESS(status))
			{
				
			}
			else
			{
				//Error
				DbgPrintLine("ERROR: (0x%X) secWow.GetSection, PID=%u", status, (ULONG)(ULONG_PTR)ProcessId);
			}
		}
#endif
	
	}
}

NTSTATUS FreeResources()
{
	// Free our resources (must be called before unloading the driver)
	NTSTATUS status = STATUS_SUCCESS;

	// Remove notification callback 
	if (_bittestandset((LONG*)&g_flags, flImageNotifySet))
	{
		status = PsRemoveLoadImageNotifyRoutine(OnLoadImage);
		if (!NT_SUCCESS(status))
		{
			DbgPrintLine("CRITICAL: (0x%X) PsSetLoadImageNotifyRoutine", status);
		}
	}
	return status;
}


void NTAPI DriverUnload(PDRIVER_OBJECT DriverObject)
{
	// Routine that is called when driver is unloaded
	NTSTATUS status = FreeResources();
	DbgPrintLine("DiverUnload(0x%p), status=0x%x", DriverObject, status);
}


extern "C" NTSTATUS NTAPI DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	DbgPrint("TESTING CUSTOM DRIVER");
	// Main Driver entry routine
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	g_DriveObject = DriverObject;

	// Debugging output function
	DbgPrintLine("DiverLoad(0x%p, %wZ)", DriverObject, RegistryPath);
	DriverObject->DriverUnload = DriverUnload;

	// Set Image loading notification routine
	NTSTATUS status = PsSetLoadImageNotifyRoutine(OnLoadImage);
	if (NT_SUCCESS(status))
	{
		_bittestandset((LONG*) & g_flags, flImageNotifySet);
	}
	else
	{
		// ERROR
		DbgPrintLine("CRITICAL : (0x%X) PsSetLoadImageNotifyRoutine", status);
	}



	return 0;
}