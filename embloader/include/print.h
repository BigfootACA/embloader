#ifndef PRINT_H
#define PRINT_H
#include <Uefi.h>
#include <stdarg.h>
#include <stddef.h>
extern EFI_STATUS text_print_repeat(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*con,char ch,size_t cnt);
extern EFI_STATUS text_print(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*con,const char*str);
extern EFI_STATUS text_printn(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*con,const char*str,size_t len);
extern EFI_STATUS text_printf(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*con,const char*str,...);
extern EFI_STATUS text_vprintf(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*con,const char*str,va_list va);
extern EFI_STATUS out_print(const char*str);
extern EFI_STATUS out_printn(const char*str,size_t len);
extern EFI_STATUS out_printf(const char*str,...);
extern EFI_STATUS out_vprintf(const char*str,va_list va);
extern EFI_STATUS err_print(const char*str);
extern EFI_STATUS err_printn(const char*str,size_t len);
extern EFI_STATUS err_printf(const char*str,...);
extern EFI_STATUS err_vprintf(const char*str,va_list va);
extern EFI_STATUS out_setattr(UINTN attr);
extern EFI_STATUS err_setattr(UINTN attr);
extern EFI_STATUS out_resetattr(void);
extern EFI_STATUS err_resetattr(void);
extern void print_init();
#define trace_line out_printf("%s %s:%d\n",__FILE__,__func__,__LINE__);
#endif
