//
// Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// This advice is specific to AMD processors and is not general guidance for all processor manufacturers.
//
// GetLogicalProcessorInformationEx requires Win7 or later

#include <intrin.h>
#include <stdio.h>
#include <windows.h>
#include <Powrprof.h>

#pragma comment( lib, "PowrProf" )

#define AMD_BULLDOZER_FAMILY 0x15

// Note that this structure definition was accidentally omitted from WinNT.h
typedef struct _PROCESSOR_POWER_INFORMATION {
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, * PPROCESSOR_POWER_INFORMATION;

// getProcessorInfo() collects the following information about the CPU(s) in the system:
//
// groups             - number of configured processor groups, usually 1
// numaNodes          - number of configured NUMA nodes, usually 1
// cores              - number of physical processor cores
// logicals           - number of logical process cores, usually 2 x cores on processors with symmetric multithreading (SMT) enabled
// maxLlcSize         - the size of the processor's last level cache, in bytes
// maxEfficiencyClass - the relationship between this processor and any other in terms of efficiency, with higher values corresponding to lower relative efficiency
//                      NOTE: maxEfficiencyClass is only non-zero on systems with a heterogeneous set of cores
void getProcessorInfo(DWORD& groups, DWORD& numaNodes, DWORD& cores, DWORD& logicals, DWORD& maxLlcSize, BYTE& maxEfficiencyClass, BOOL forceSingleNumaNode = false) {
	// Consider all processors in the system with a fully set affinity mask
	GROUP_AFFINITY filterGroupAffinity = { static_cast<KAFFINITY>(0xffffffffffffffff), 0 };

	if (forceSingleNumaNode) {
		PROCESSOR_NUMBER ProcNum;
		USHORT FilterNodeNumber;
		GetThreadIdealProcessorEx(GetCurrentThread(), &ProcNum);
		GetNumaProcessorNodeEx(&ProcNum, &FilterNodeNumber);
		GetNumaNodeProcessorMaskEx(FilterNodeNumber, &filterGroupAffinity);
	}

	groups = numaNodes = cores = logicals = maxLlcSize = maxEfficiencyClass = 0;
	char* buffer = NULL;
	DWORD len = 0;

	if (FALSE == GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len)) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			buffer = (char*)malloc(len);

			if (GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len)) {
				const char* ptr = buffer;

				while (ptr < buffer + len) {
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pi = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;

					if (NULL == pi) {
						break;
					}

					if (pi->Relationship == RelationProcessorCore) {
						if (forceSingleNumaNode) {
							for (size_t g = 0; g < pi->Processor.GroupCount; ++g) {
								if (filterGroupAffinity.Group == pi->Processor.GroupMask[g].Group) {
									KAFFINITY intersection = filterGroupAffinity.Mask & pi->Processor.GroupMask[g].Mask;

									if (intersection > 0) {
										cores++;
										logicals += static_cast<DWORD>(__popcnt64(intersection));
									}
								}
							}
						}
						else {
							cores++;

							for (size_t g = 0; g < pi->Processor.GroupCount; ++g) {
								logicals += static_cast<DWORD>(__popcnt64(pi->Processor.GroupMask[g].Mask));
							}
						}

						if (pi->Processor.EfficiencyClass > maxEfficiencyClass) {
							maxEfficiencyClass = pi->Processor.EfficiencyClass;
						}
					}

					if (pi->Relationship == RelationNumaNode) {
						numaNodes++;
					}

					if (pi->Relationship == RelationGroup) {
						groups = pi->Group.ActiveGroupCount;
					}

					if (pi->Relationship == RelationCache) {
						if (pi->Cache.CacheSize > maxLlcSize) {
							maxLlcSize = pi->Cache.CacheSize;
						}
					}

					ptr += pi->Size;
				}
			}

			free(buffer);
		}
	}
}

// wrapper for getProcessorInfo() that only collects physical and logical core counts
void getProcessorCount(DWORD& cores, DWORD& logicals) {
	DWORD groups, numaNodes, maxLlcSize;
	BYTE maxEfficiencyClass;
	getProcessorInfo(groups, numaNodes, cores, logicals, maxLlcSize, maxEfficiencyClass);
}

// Get the processor name string via cpuid instruction intrinsic
//
// name - processor name is a null-terminated byte string of length 49 including the null character
const char* getCpuidName(char* name) {
	name[0] = 0;
	int data[4];

	__cpuid(data, 0x80000000);

	if (data[0] >= 0x80000004) {
		for (unsigned long long i = 0; i < 3; ++i) {
			__cpuid(data, (int)(0x80000002 + i));
			*reinterpret_cast<int*>(name + 0 + 16 * i) = data[0];
			*reinterpret_cast<int*>(name + 4 + 16 * i) = data[1];
			*reinterpret_cast<int*>(name + 8 + 16 * i) = data[2];
			*reinterpret_cast<int*>(name + 12 + 16 * i) = data[3];
		}
		name[48] = 0;
	}

	return name;
}

// Get the processor vendor name string via cpuid instruction intrinsic
//
// vendor - processor vendor is a null-terminated byte string of length 13 including the null character
const char* getCpuidVendor(char* vendor) {
	int data[4];
	__cpuid(data, 0);

	*reinterpret_cast<int*>(vendor) = data[1];
	*reinterpret_cast<int*>(vendor + 4) = data[3];
	*reinterpret_cast<int*>(vendor + 8) = data[2];
	vendor[12] = 0;

	return vendor;
}

// Get the processor family via cpuid instruction intrinsic
int getCpuidFamily() {
	int data[4];
	__cpuid(data, 1);

	int family = ((data[0] >> 8) & 0x0F);
	int extendedFamily = (data[0] >> 20) & 0xFF;

	int displayFamily = (family != 0x0F) ? family : (extendedFamily + family);

	return displayFamily;
}

// ##################################################################################################################
// ### This advice is specific only to AMD processors and is NOT general guidance for all processor manufacturers ###
// ###                                                                                                            ###
// ###                                       Remember to profile!                                                 ###
// ##################################################################################################################

// Return a recommended number of hardware threads to use for running your game, taking into account processor family and configuration
// For Ryzen processors with a number of physical cores below the configured threshold, logical processor cores are added to the recommended thread count
#define RYZEN_CORES_THRESHOLD 8

DWORD getRecommendedThreadCountForGameplay(BOOL forceSingleNumaNode = false, BOOL forceSMT = false, DWORD maxThreadPoolSize = MAXUINT32, DWORD forceThreadPoolSize = 0) {
	DWORD groups, numaNodes, cores, logicals, maxLlcSize;
	BYTE maxEfficiencyClass;

	getProcessorInfo(groups, numaNodes, cores, logicals, maxLlcSize, maxEfficiencyClass, forceSingleNumaNode);
	DWORD count = logicals;

	char vendor[13];
	getCpuidVendor(vendor);

	if (0 == strcmp(vendor, "AuthenticAMD")) {
		if (AMD_BULLDOZER_FAMILY == getCpuidFamily()) {
			// Use the reported logical processor count on AMD "Bulldozer" family microarchitecture processors
			count = logicals;
		}
		else {
			// Use the physical core count, unless the number of physical cores is lower than the defined threshold
			count = (cores >= RYZEN_CORES_THRESHOLD) ? cores : logicals;
		}
	}

	// take into account SMT when calculating thread count
	if (forceSMT) {
		count = logicals;
	}

	// clamp the thread count to at most the size of maxThreadPoolSize
	if (maxThreadPoolSize > 0) {
		count = min(count, maxThreadPoolSize);
	}

	// force a particular thread count
	if (forceThreadPoolSize) {
		count = forceThreadPoolSize;
	}

	// always return at least 1 just in case count is 0 by the time we get here
	return max(1, count);
}

// ##################################################################################################################
// ### This advice is specific only to AMD processors and is NOT general guidance for all processor manufacturers ###
// ###                                                                                                            ###
// ###                                       Remember to profile!                                                 ###
// ##################################################################################################################

// Return a recommended number of hardware threads to use for initialising your game, taking into account processor family and configuration     
DWORD getRecommendedThreadCountForGameInit(BOOL forceSingleNumaNode = false, BOOL forceSMT = false, DWORD maxThreadPoolSize = MAXUINT32, DWORD forceThreadPoolSize = 0) {
	DWORD groups, numaNodes, cores, logicals, maxLlcSize;
	BYTE maxEfficiencyClass;

	getProcessorInfo(groups, numaNodes, cores, logicals, maxLlcSize, maxEfficiencyClass, forceSingleNumaNode);
	DWORD count = logicals;

	// take into account SMT when calculating thread count
	if (forceSMT) {
		count = logicals;
	}

	// clamp the thread count to at most the size of maxThreadPoolSize
	if (maxThreadPoolSize > 0) {
		count = min(count, maxThreadPoolSize);
	}

	// force a particular thread count
	if (forceThreadPoolSize) {
		count = forceThreadPoolSize;
	}

	// always return at least 1 just in case count is 0 by the time we get here
	return max(1, count);
}

// Print all of the processor information
void printProcessorInfo() {
	char name[49];
	getCpuidName(name);

	char vendor[13];
	getCpuidVendor(vendor);

	DWORD groups, numaNodes, cores, logicals, maxLlcSize;
	BYTE maxEfficiencyClass;
	getProcessorInfo(groups, numaNodes, cores, logicals, maxLlcSize, maxEfficiencyClass);

	int processorFamily = getCpuidFamily();

	wprintf(L"Processor Name: %hs\n", name);
	wprintf(L"Processor Vendor: %hs\n", vendor);
	wprintf(L"Processor Family: 0x%x\n", processorFamily);
	wprintf(L"Processor Group Count: %lu\n", groups);
	wprintf(L"NUMA Node Count: %lu\n", numaNodes);

	if ((0 == strcmp(vendor, "AuthenticAMD")) && (AMD_BULLDOZER_FAMILY == processorFamily)) {
		// Print module count for AMD "Bulldozer" family microarchitecture processors
		wprintf(L"Processor Module Count: %lu\n", logicals / 2);
		wprintf(L"Processor Core Count: %lu\n", logicals);
	}
	else {
		wprintf(L"Processor Core Count: %lu\n", cores);
	}

	wprintf(L"Logical Processor Count: %lu\n", logicals);
	wprintf(L"Max Last Level Cache Size: %lu Bytes\n", maxLlcSize);
	wprintf(L"Max Processor Efficiency Class: 0x%#02x\n", maxEfficiencyClass); // See the getProcessorInfo() documentation for more information on processor efficiency classes

	// If available, print the value of PROCESSOR_POWER_INFORMATION.MaxMhz provided by CallNtPowerInformation(ProcessorInformation)
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		void* buffer = malloc(sizeof(PROCESSOR_POWER_INFORMATION) * info.dwNumberOfProcessors);

		if (buffer && 0 == CallNtPowerInformation(ProcessorInformation, NULL, 0, buffer, sizeof(PROCESSOR_POWER_INFORMATION) * info.dwNumberOfProcessors)) {
			PROCESSOR_POWER_INFORMATION pi = ((PROCESSOR_POWER_INFORMATION*)buffer)[0];
			wprintf(L"MaxMhz: %lu MHz\n", pi.MaxMhz); // This is typically the processor's base clock
		}

		if (buffer) {
			free(buffer);
		}
	}
}

int main(int argc, char* argv[]) {
	wprintf(L"%hs [forceSingleNumaNode] [forceSMT] [maxThreadPoolSize] [forceThreadPoolSize]\n", argv[0]);

	printProcessorInfo();

	BOOL forceSingleNumaNode = (argc > 1) ? atoi(argv[1]) : 0;
	BOOL forceSMT = (argc > 2) ? atoi(argv[2]) : 0;
	DWORD maxThreadPoolSize = (argc > 3) ? strtoul(argv[3], NULL, 0) : 0U;
	DWORD forceThreadPoolSize = (argc > 4) ? strtoul(argv[4], NULL, 0) : 0U;
	DWORD initThreads = getRecommendedThreadCountForGameInit(forceSingleNumaNode, forceSMT, maxThreadPoolSize, forceThreadPoolSize);
	DWORD playThreads = getRecommendedThreadCountForGameplay(forceSingleNumaNode, forceSMT, maxThreadPoolSize, forceThreadPoolSize);

	wprintf(L"forceSingleNumaNode: %i, forceSMT: %i, maxThreadPoolSize: %lu, forceThreadPoolSize: %lu\n", forceSingleNumaNode, forceSMT, maxThreadPoolSize, forceThreadPoolSize);
	wprintf(L"AMD Recommended Game Init Thread Count: %lu\n", initThreads);
	wprintf(L"AMD Recommended Game Play Thread Count: %lu\n", playThreads);

	return 0;
}
