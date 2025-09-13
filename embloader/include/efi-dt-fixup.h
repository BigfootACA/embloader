#ifndef EFI_DT_FIXUP_H
#define EFI_DT_FIXUP_H
#include <Uefi.h>
#define EFI_DT_FIXUP_PROTOCOL_GUID { 0xe617d64c, 0xfe08, 0x46da, { 0xf4, 0xdc, 0xbb, 0xd5, 0x87, 0x0c, 0x73, 0x00 } }
#define EFI_DT_FIXUP_PROTOCOL_REVISION 0x00010000

typedef struct _EFI_DT_FIXUP_PROTOCOL EFI_DT_FIXUP_PROTOCOL;

typedef EFI_STATUS
(EFIAPI *EFI_DT_FIXUP) (
	IN EFI_DT_FIXUP_PROTOCOL *This,
	IN VOID                  *Fdt,
	IN OUT UINTN             *BufferSize,
	IN UINT32                Flags
);
struct _EFI_DT_FIXUP_PROTOCOL {
	UINT64         Revision;
	EFI_DT_FIXUP   Fixup;
};

#define EFI_DT_APPLY_FIXUPS    0x00000001
#define EFI_DT_RESERVE_MEMORY  0x00000002
extern EFI_GUID gEfiDtFixupProtocolGuid;
#endif
