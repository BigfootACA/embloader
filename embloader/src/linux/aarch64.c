#include <Library/ArmLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "linuxboot.h"
#include "efi-utils.h"
#include "libfdt.h"
#include "log.h"

#define ARM64_IMAGE_MAGIC 0x644d5241
#define ARM64_IMAGE_ALIGNMENT SIZE_2MB
#define ARM64_IMAGE_CAN_BE_PLACED_ANYWHERE BIT3
#define ARM64_FDT_MAX_SIZE SIZE_2MB
#define EXIT_BOOT_SERVICES_RETRIES 4

struct arm64_image_header {
	UINT32 code0;
	UINT32 code1;
	UINT64 text_offset;
	UINT64 image_size;
	UINT64 flags;
	UINT64 res2;
	UINT64 res3;
	UINT64 res4;
	UINT32 magic;
	UINT32 res5;
};

typedef void (*linux_kernel_aarch64_main)(UINT64 fdt_address, UINT64 zero1, UINT64 zero2, UINT64 zero3);

static EFI_STATUS linux_exit_boot_services(void) {
	EFI_MEMORY_DESCRIPTOR* map = NULL;
	EFI_STATUS status;
	UINTN map_size = 0, map_capacity = 0, map_key = 0, descriptor_size = 0;
	UINT32 descriptor_version = 0;

	for (
		UINTN attempt = 0; attempt < EXIT_BOOT_SERVICES_RETRIES;
		attempt++
	) {
		map_size = map_capacity;
		status = gBS->GetMemoryMap(
			&map_size, map, &map_key, &descriptor_size, &descriptor_version
		);
		if (status == EFI_BUFFER_TOO_SMALL) {
			if (map) FreePool(map);
			map = NULL;
			map_capacity = map_size + (8 * descriptor_size);
			map = AllocatePool(map_capacity);
			if (!map) return EFI_OUT_OF_RESOURCES;
			continue;
		}
		if (EFI_ERROR(status)) break;

		status = gBS->ExitBootServices(gImageHandle, map_key);
		if (!EFI_ERROR(status)) return EFI_SUCCESS;
		if (status != EFI_INVALID_PARAMETER) break;
	}

	if (map) FreePool(map);
	return status;
}

static EFI_STATUS invoke_boot(
	void* kernel, UINTN kernel_size, void* fdt, UINTN fdt_size,
	void* initrd, UINTN initrd_size, linux_before_handoff before_handoff
) {
	EFI_STATUS status;
	linux_kernel_aarch64_main entry = (linux_kernel_aarch64_main) kernel;

	log_info("booting Linux Image at %p with fdt at %p", kernel, fdt);
	if (before_handoff) (void) before_handoff();
	status = linux_exit_boot_services();
	if (EFI_ERROR(status)) return status;

	/* The kernel sees physical addresses after the MMU is disabled. */
	ArmDisableInterrupts();
	WriteBackDataCacheRange(kernel, kernel_size);
	WriteBackDataCacheRange(fdt, fdt_size);
	if (initrd && initrd_size) WriteBackDataCacheRange(initrd, initrd_size);
	InvalidateInstructionCacheRange(kernel, kernel_size);
	ArmDisableDataCache();
	ArmDisableInstructionCache();
	ArmDisableMmu();
	entry((UINT64) (UINTN) fdt, 0, 0, 0);
	while (true)
		asm volatile("wfe");
}

static bool include_memory_descriptor(const EFI_MEMORY_DESCRIPTOR* descriptor) {
	if (descriptor->NumberOfPages == 0) return false;
	if (descriptor->Attribute & EFI_MEMORY_RUNTIME) return false;
	switch (descriptor->Type) {
		case EfiLoaderCode:
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiConventionalMemory:
			return true;
		default:
			return false;
	}
}

static EFI_STATUS load_memory_map(struct linux_memory** memory, size_t* count) {
	EFI_MEMORY_DESCRIPTOR *map = NULL, *descriptor;
	EFI_STATUS status;
	UINTN map_size = 0, map_key, descriptor_size = 0, capacity = 0;
	UINT32 descriptor_version;
	size_t found = 0, output = 0;

	if (!memory || !count) return EFI_INVALID_PARAMETER;
	*memory = NULL;
	*count = 0;

	for (;;) {
		map_size = capacity;
		status = gBS->GetMemoryMap(
			&map_size, map, &map_key, &descriptor_size, &descriptor_version
		);
		if (status != EFI_BUFFER_TOO_SMALL) break;
		if (map) FreePool(map);
		capacity = map_size + (2 * descriptor_size);
		map = AllocatePool(capacity);
		if (!map) return EFI_OUT_OF_RESOURCES;
	}
	if (EFI_ERROR(status)) {
		if (map) FreePool(map);
		return status;
	}

	for (
		descriptor = map; (UINT8*) descriptor < (UINT8*) map + map_size;
		descriptor = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) descriptor + descriptor_size)
	)
		if (include_memory_descriptor(descriptor)) found++;
	if (found == 0) {
		FreePool(map);
		return EFI_NOT_FOUND;
	}

	*memory = AllocatePool(found * sizeof(**memory));
	if (!*memory) {
		FreePool(map);
		return EFI_OUT_OF_RESOURCES;
	}
	for (
		descriptor = map; (UINT8*) descriptor < (UINT8*) map + map_size;
		descriptor = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) descriptor + descriptor_size)
	) {
		EFI_PHYSICAL_ADDRESS end;
		if (!include_memory_descriptor(descriptor)) continue;
		if (
			descriptor->NumberOfPages > MAX_UINT64 / EFI_PAGE_SIZE ||
			descriptor->PhysicalStart > MAX_UINT64 - descriptor->NumberOfPages * EFI_PAGE_SIZE
		) {
			FreePool(map);
			FreePool(*memory);
			*memory = NULL;
			return EFI_COMPROMISED_DATA;
		}
		end = descriptor->PhysicalStart + descriptor->NumberOfPages * EFI_PAGE_SIZE;
		if (output && (*memory)[output - 1].address + (*memory)[output - 1].size == descriptor->PhysicalStart) {
			(*memory)[output - 1].size = end - (*memory)[output - 1].address;
		} else {
			(*memory)[output].address = descriptor->PhysicalStart;
			(*memory)[output].size = end - descriptor->PhysicalStart;
			output++;
		}
	}
	FreePool(map);
	for (size_t i = 1; i < output; i++) {
		struct linux_memory current = (*memory)[i];
		size_t j = i;
		while (j > 0 && (*memory)[j - 1].address > current.address) {
			(*memory)[j] = (*memory)[j - 1];
			j--;
		}
		(*memory)[j] = current;
	}
	if (output > 1) {
		size_t merged = 0;
		for (size_t i = 1; i < output; i++) {
			UINT64 merged_end = (*memory)[merged].address + (*memory)[merged].size;
			UINT64 current_end = (*memory)[i].address + (*memory)[i].size;
			if (merged_end >= (*memory)[i].address) {
				if (current_end > merged_end)
					(*memory)[merged].size = current_end - (*memory)[merged].address;
			} else {
				merged++;
				(*memory)[merged] = (*memory)[i];
			}
		}
		output = merged + 1;
	}
	*count = output;
	return EFI_SUCCESS;
}

static EFI_STATUS allocate_legacy_kernel_pages(UINTN pages, void** allocation) {
	EFI_MEMORY_DESCRIPTOR *map = NULL, *descriptor;
	EFI_PHYSICAL_ADDRESS candidate = MAX_UINT64;
	EFI_STATUS status;
	UINTN map_size = 0, map_key, descriptor_size = 0, capacity = 0;
	UINT64 allocation_size = EFI_PAGES_TO_SIZE(pages);
	UINT32 descriptor_version;

	if (!allocation || pages == 0) return EFI_INVALID_PARAMETER;
	*allocation = NULL;
	for (;;) {
		map_size = capacity;
		status = gBS->GetMemoryMap(
			&map_size, map, &map_key, &descriptor_size, &descriptor_version
		);
		if (status != EFI_BUFFER_TOO_SMALL) break;
		if (map) FreePool(map);
		capacity = map_size + (2 * descriptor_size);
		map = AllocatePool(capacity);
		if (!map) return EFI_OUT_OF_RESOURCES;
	}
	if (EFI_ERROR(status)) {
		if (map) FreePool(map);
		return status;
	}

	/* Pre-v4.6 kernels cannot linearly map RAM below the Image base. */
	for (
		descriptor = map; (UINT8*) descriptor < (UINT8*) map + map_size;
		descriptor = (EFI_MEMORY_DESCRIPTOR*) ((UINT8*) descriptor + descriptor_size)
	) {
		EFI_PHYSICAL_ADDRESS start, end;
		if (descriptor->Type != EfiConventionalMemory || descriptor->NumberOfPages > MAX_UINT64 / EFI_PAGE_SIZE) continue;
		if (descriptor->PhysicalStart > MAX_UINT64 - descriptor->NumberOfPages * EFI_PAGE_SIZE) continue;
		end = descriptor->PhysicalStart + descriptor->NumberOfPages * EFI_PAGE_SIZE;
		if (descriptor->PhysicalStart > MAX_UINT64 - ((EFI_PHYSICAL_ADDRESS) ARM64_IMAGE_ALIGNMENT - 1)) continue;
		start = (descriptor->PhysicalStart + (EFI_PHYSICAL_ADDRESS) ARM64_IMAGE_ALIGNMENT - 1) & ~((EFI_PHYSICAL_ADDRESS) ARM64_IMAGE_ALIGNMENT - 1);
		if (start < candidate && start <= end && allocation_size <= end - start)
			candidate = start;
	}
	FreePool(map);
	if (candidate == MAX_UINT64) return EFI_OUT_OF_RESOURCES;

	status = gBS->AllocatePages(
		AllocateAddress, EfiLoaderData, pages, &candidate);
	if (EFI_ERROR(status)) return status;
	*allocation = (void*) (UINTN) candidate;
	return EFI_SUCCESS;
}

static EFI_STATUS prepare_kernel(
	const linux_data* data, void** allocation, void** kernel,
	UINTN* allocation_pages, UINTN* image_size, bool* allocation_aligned
) {
	const struct arm64_image_header* header;
	EFI_STATUS status;
	UINTN footprint, pages;

	if (
		!data || !data->kernel || data->kernel_size < sizeof(*header) ||
		!allocation || !kernel || !allocation_pages || !image_size ||
		!allocation_aligned
	)
		return EFI_LOAD_ERROR;
	*allocation_aligned = false;
	header = data->kernel;
	if (header->magic != ARM64_IMAGE_MAGIC) {
		log_error("kernel is not a valid AArch64 Linux Image");
		return EFI_LOAD_ERROR;
	}
	if (header->image_size > MAX_UINTN) return EFI_BAD_BUFFER_SIZE;
	footprint = MAX(data->kernel_size, (UINTN) header->image_size);
	if (header->text_offset > MAX_UINTN || (UINTN) header->text_offset > MAX_UINTN - footprint)
		return EFI_BAD_BUFFER_SIZE;
	UINTN allocation_size = (UINTN) header->text_offset + footprint;
	if (allocation_size > MAX_UINTN - EFI_PAGE_SIZE + 1)
		return EFI_BAD_BUFFER_SIZE;
	pages = EFI_SIZE_TO_PAGES(allocation_size);
	if (header->flags & ARM64_IMAGE_CAN_BE_PLACED_ANYWHERE) {
		*allocation = AllocateAlignedPages(pages, ARM64_IMAGE_ALIGNMENT);
		if (!*allocation) return EFI_OUT_OF_RESOURCES;
		*allocation_aligned = true;
	} else {
		status = allocate_legacy_kernel_pages(pages, allocation);
		if (EFI_ERROR(status)) return status;
	}
	*kernel = (UINT8*) *allocation + (UINTN) header->text_offset;
	memset(*kernel, 0, footprint);
	memcpy(*kernel, data->kernel, data->kernel_size);
	*allocation_pages = pages;
	*image_size = footprint;
	return EFI_SUCCESS;
}

EFI_STATUS linux_boot_use_aarch64(
	linux_data* data,
	linux_before_handoff before_handoff
) {
	EFI_STATUS status;
	struct linux_memory* memory = NULL;
	void *kernel_allocation = NULL, *kernel = NULL;
	void *fdt = NULL, *source_fdt;
	UINTN kernel_pages = 0, kernel_size = 0, fdt_size;
	bool kernel_allocation_aligned = false;
	size_t memory_count = 0;
	int ret;

	if (!data) return EFI_INVALID_PARAMETER;
	if (!data->fdt && !g_embloader.fdt) {
		status = embloader_prepare_boot();
		if (EFI_ERROR(status)) return status;
	}
	source_fdt = data->fdt ?: g_embloader.fdt;
	if (!source_fdt || fdt_check_header(source_fdt) != 0) {
		log_error("no valid device tree loaded");
		return EFI_LOAD_ERROR;
	}

	status = prepare_kernel(
		data, &kernel_allocation, &kernel, &kernel_pages, &kernel_size,
		&kernel_allocation_aligned
	);
	if (EFI_ERROR(status)) goto done;
	fdt = AllocateAlignedPages(
		EFI_SIZE_TO_PAGES(ARM64_FDT_MAX_SIZE), ARM64_FDT_MAX_SIZE
	);
	if (!fdt) {
		status = EFI_OUT_OF_RESOURCES;
		goto done;
	}
	ret = fdt_open_into(source_fdt, fdt, ARM64_FDT_MAX_SIZE);
	if (ret != 0) {
		log_error("failed to copy device tree: %s", fdt_strerror(ret));
		status = EFI_LOAD_ERROR;
		goto done;
	}
	if (data->bootargs && (ret = linux_fdt_fill_bootargs(fdt, data->bootargs)) != 0) {
		log_error("failed to set bootargs in device tree: %s", fdt_strerror(ret));
		status = EFI_LOAD_ERROR;
		goto done;
	}
	ret = linux_fdt_fill_initrd(fdt, data->initramfs, data->initramfs_size);
	if (ret != 0) {
		log_error("failed to set initramfs in device tree: %s", fdt_strerror(ret));
		status = EFI_LOAD_ERROR;
		goto done;
	}
	status = load_memory_map(&memory, &memory_count);
	if (EFI_ERROR(status)) {
		log_error("failed to obtain system memory map: %s", efi_status_to_string(status));
		goto done;
	}
	if (
		(ret = linux_fdt_fill_memory(fdt, memory, memory_count)) != 0 ||
		(ret = fdt_pack(fdt)) != 0
	) {
		log_error("failed to finalize device tree: %s", fdt_strerror(ret));
		status = EFI_LOAD_ERROR;
		goto done;
	}
	fdt_size = fdt_totalsize(fdt);
	status = invoke_boot(
		kernel, kernel_size, fdt, fdt_size,
		data->initramfs, data->initramfs_size, before_handoff
	);

done:
	if (memory) FreePool(memory);
	if (fdt) FreeAlignedPages(fdt, EFI_SIZE_TO_PAGES(ARM64_FDT_MAX_SIZE));
	if (kernel_allocation) {
		if (kernel_allocation_aligned)
			FreeAlignedPages(kernel_allocation, kernel_pages);
		else
			FreePages(kernel_allocation, kernel_pages);
	}
	return status;
}
