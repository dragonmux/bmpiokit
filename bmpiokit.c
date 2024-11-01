// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2024 1BitSquared <info@1bitsquared.com>
// SPDX-FileContributor: Written by Rachel Mant <git@dragonmux.network>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <uchar.h>
#include <mach/mach.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

static SInt16 bmdVID = 0x1d50;
static SInt16 bmdPID = 0x6018;

mach_port_t openIOKitInterface(void)
{
	mach_port_t ioKitPort = MACH_PORT_NULL;
	const kern_return_t result = IOMainPort(MACH_PORT_NULL, &ioKitPort);
	if (result != KERN_SUCCESS || ioKitPort == MACH_PORT_NULL)
	{
		printf("Failed to initiate comms with IOKit: %08x\n", result);
		return MACH_PORT_NULL;
	}
	return ioKitPort;
}

CFMutableDictionaryRef buildBMPMatchingDict(void)
{
	// Start by creating a new dictionary for matching on the IOKit IOUSBDevice base class
	CFMutableDictionaryRef dict = IOServiceMatching(kIOUSBDeviceClassName);
	if (!dict)
	{
		printf("Failed to allocate USB device matching dictionary\n");
		return NULL;
	}

	// Now also add matching on the specific desired VID:PID
	CFDictionarySetValue(dict, CFSTR(kUSBVendorName), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &bmdVID));
	CFDictionarySetValue(dict, CFSTR(kUSBProductName), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &bmdPID));

	return dict;
}

io_iterator_t discoverProbes(const mach_port_t ioKitPort)
{
	// Next, set up the device matching dictionary to find BMPs with
	const CFMutableDictionaryRef deviceMatchingDict = buildBMPMatchingDict();
	if (deviceMatchingDict == NULL)
		return MACH_PORT_NULL;

	// Now find all devices matching our dictionary on the system
	io_iterator_t matches = MACH_PORT_NULL;
	// NB, this call consumes deviceMatchingDict.
	const kern_return_t result = IOServiceGetMatchingServices(ioKitPort, deviceMatchingDict, &matches);
	if (result != KERN_SUCCESS)
	{
		printf("Failed to run USB device matching: %08x\n", result);
		return MACH_PORT_NULL;
	}

	return matches;
}

IOUSBDeviceInterface **openDevice(const io_service_t usbDeviceService)
{
	// Check that the service is valid
	if (usbDeviceService == MACH_PORT_NULL)
		return NULL;

	// As it is, create a CoreFoundation plug-in client for the device sos we can get a step closer to accessing it
	IOCFPlugInInterface **pluginInterface = NULL;
	{
		SInt32 score; // XXX: No idea what this is/does - does it matter? Can we skip it? etc.. fruitco doesn't document it.
		const kern_return_t result = IOCreatePlugInInterfaceForService(usbDeviceService,
			kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &pluginInterface, &score);
		// Clean up now we're done with the device
		IOObjectRelease(usbDeviceService);
		// Check how we got on, bailing if anything went wrong
		if (result != kIOReturnSuccess || pluginInterface == NULL)
		{
			printf("Failed to create client plug-in binding: %08x\n", result);
			return NULL;
		}
	}

	// Now we've got the stepping stone for this, create the actual device interface instance for the device
	IOUSBDeviceInterface **deviceInterface = NULL;
	const HRESULT result = (*pluginInterface)->QueryInterface(pluginInterface,
		CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (void **)&deviceInterface);
	// Clean up now we're done with the stepping stone plugin client interface
	(*pluginInterface)->Release(pluginInterface);
	// See how things went, bailing if that didn't work
	if (result || deviceInterface == NULL)
	{
		printf("Failed to create an interface to the device: %08x\n", (int)result);
		return NULL;
	}

	return deviceInterface;
}

uint8_t requestStringLength(IOUSBDeviceInterface **const usbDevice, const uint8_t index)
{
	// Request just the first couple of bytes of the descriptor to validate and grab the length byte from
	uint8_t data[2U] = {0U};
	const IOUSBDevRequestTO request =
	{
		.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice),
		.bRequest = kUSBRqGetDescriptor,
		.wValue = kUSBStringDesc,
		.wIndex = index,
		.wLength = sizeof(data),
		.pData = data,
		.noDataTimeout = 20,
		.completionTimeout = 100,
	};

	// Make the request, check that it was successful, and that we got a string descriptor back
	const IOReturn result = (*usbDevice)->DeviceRequestTO(device, &request);
	if (result != kIOSuccess || data[1U] != kUSBStringDesc)
		return 0U;
	// Convert the length field from a length in bytes to a length in UTF-16 code units
	return (data[0U] - 2U) / 2U;
}

IOReturn requestStringDescriptor(IOUSBDeviceInterface **const usbDevice, const uint8_t index, char16_t *const string, const uint8_t length)
{
	// Check that the string length isn't too long, and bail if it is
	if (length > 127U)
		return kIOReturnBadArgument;
	uint8_t data[256U] = {0U};
	const IOUSBDevRequestTO request =
	{
		.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice),
		.bRequest = kUSBRqGetDescriptor,
		.wValue = kUSBStringDesc,
		.wIndex = index,
		// Convert the length in UTF-16 code units to a length in bytes and include the 2 byte descriptor header
		.wLength = (length * 2U) + 2U,
		.pData = data,
		.noDataTimeout = 20,
		.completionTimeout = 100,
	};

	// Make the request, check that it was successful, and that we got a string descriptor back
	const IOReturn result = (*usbDevice)->DeviceRequestTO(device, &request);
	if (result != kIOSuccess)
		return result;
	if (data[1U] != kUSBStringDesc)
		return kIOReturnError;

	/* Having extracted the string, check how many bytes we actually have before prepping to copy them to the result string */
	const uint8_t validBytes = MIN(request[0U], length * 2U);
	memcpy(string, result + 2U, validBytes);
	return kIOResultSuccess;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	// Start by getting an interface with IOKit
	const mach_port_t ioKitPort = openIOKitInterface();
	if (ioKitPort == MACH_PORT_NULL)
		return 1;

	// Now try to get an iterator for all available BMPs on the syste
	const io_iterator_t deviceIterator = discoverProbes(ioKitPort);
	mach_port_deallocate(mach_task_self(), ioKitPort);
	if (deviceIterator == MACH_PORT_NULL)
		return 1;

	// Early exit if we found no BMPs
	if (!IOIteratorIsValid(deviceIterator))
	{
		printf("No BMPs found on system\n");
		return 1;
	}

	// Loop through all the devices matched, poking them one at a time
	for (; IOIteratorIsValid(deviceIterator); )
	{
		IOUSBDeviceInterface **const usbDevice = openDevice(IOIteratorNext(deviceIterator));
		if (usbDevice == NULL)
			break;

		USBDeviceAddress busAddress;
		(*usbDevice)->GetDeviceAddress(usbDevice, &busAddress);
		uint16_t vid;
		(*usbDevice)->GetDeviceVendor(usbDevice, &vid);
		uint16_t pid;
		(*usbDevice)->GetDeviceProduct(usbDevice, &pid);

		printf("Found device at address %u with VID:PID %04x:%04x\n", busAddress, vid, pid);

		// Finish up by releasing the device
		(*usbDevice)->Release(usbDevice);
	}

	return 0;
}
