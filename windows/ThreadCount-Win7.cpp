// This advice is specific to AMD processors and is not general guidance for all processor manufacturers.
//
// GetLogicalProcessorInformationEx requires Win7 or later
// based on https://msdn.microsoft.com/en-us/library/windows/desktop/dd405488(v=vs.85).aspx

#include <stdio.h>
#include <intrin.h>
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
	char* buffer = NULL;
	DWORD len = 0;
	if (FALSE == GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len)) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			buffer = (char*)malloc(len);
			if (GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len)) {
				DWORD offset = 0;
				char* ptr = buffer;
				while (ptr < buffer + len) {
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pi = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
					if (pi->Relationship == RelationProcessorCore) {
						cores++;
						for (size_t g = 0; g < pi->Processor.GroupCount; ++g) {
							logical += CountSetBits(pi->Processor.GroupMask[g].Mask);
						}
					}
					ptr += pi->Size;
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

// This advice is specific to AMD processors and is
// not general guidance for all processor
// manufacturers. Remember to profile!
DWORD getDefaultThreadCount() {
	DWORD cores, logical;
	getProcessorCount(cores, logical);
	DWORD count = logical;
	char vendor[13];
	getCpuidVendor(vendor);
	if (0 == strcmp(vendor, "AuthenticAMD")) {
		if (0x15 == getCpuidFamily()) {
			// AMD "Bulldozer" family microarchitecture
			count = logical;
		}
		else {
			count = cores;
		}
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
