// based on https://msdn.microsoft.com/en-us/library/windows/desktop/ms683194(v=vs.85).aspx

#include <intrin.h>
#include <stdio.h>
#include <windows.h>

DWORD CountSetBits(ULONG_PTR bitMask) {
	DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	for (DWORD i = 0; i <= LSHIFT; ++i) {
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}
	return bitSetCount;
}

void getProcessorCount(DWORD& cores, DWORD& logical) {
	cores = logical = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	DWORD len = 0;
	if (FALSE == GetLogicalProcessorInformation(buffer, &len)) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(len);
			if (GetLogicalProcessorInformation(buffer, &len)) {
				DWORD offset = 0;
				PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
				while (offset < len) {
					if (ptr->Relationship == RelationProcessorCore) {
						cores++;
						logical += CountSetBits(ptr->ProcessorMask);
					}
					offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
					ptr++;
				}
			}
			free(buffer);
		}
	}
}

char* getCpuidVendor(char* vendor) {
	int data[4];
	__cpuid(data, 0);
	*reinterpret_cast<int*>(vendor) = data[1];
	*reinterpret_cast<int*>(vendor + 4) = data[3];
	*reinterpret_cast<int*>(vendor + 8) = data[2];
	vendor[12] = 0;
	return vendor;
}

int getCpuidFamily() {
	int data[4];
	__cpuid(data, 1);
	int family = ((data[0] >> 8) & 0x0F);
	int extendedFamily = (data[0] >> 20) & 0xFF;
	int displayFamily = (family != 0x0F) ? family : (extendedFamily + family);
	return displayFamily;
}

DWORD getDefaultThreadCount() {
	DWORD cores, logical;
	getProcessorCount(cores, logical);
	DWORD count = cores;
	char vendor[13];
	getCpuidVendor(vendor);
	if ((0 == strcmp(vendor, "AuthenticAMD")) && (0x15 == getCpuidFamily())) {
		// AMD "Bulldozer" family microarchitecture
		count = logical;
	}
	return count;
}

int main(int argc, char* argv[]) {
	char vendor[13];
	getCpuidVendor(vendor);
	printf("Vendor: %s\n", vendor);
	printf("Family: %x\n", getCpuidFamily());

	DWORD cores, logical;
	getProcessorCount(cores, logical);
	if ((0 == strcmp(vendor, "AuthenticAMD")) && (0x15 == getCpuidFamily())) {
		// AMD "Bulldozer" family microarchitecture
		printf("Processor Module Count: %u\n", logical / 2);
		printf("Processor Core Count: %u\n", logical);
	}
	else {
		printf("Processor Core Count: %u\n", cores);
	}
	printf("Logical Processor Count: %u\n", logical);

	DWORD threads = getDefaultThreadCount();
	printf("Default Thread Count: %u\n", getDefaultThreadCount());

	return 0;
}