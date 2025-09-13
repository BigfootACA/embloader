.globl mouse_cursor_icon
.globl mouse_cursor_icon_size

.balign 8
.section .data
mouse_cursor_icon:
	.incbin "mouse.svg"
	.byte 0
.Lendmouse_cursor_icon_end:
	.balign 8
mouse_cursor_icon_size:
	.quad .Lendmouse_cursor_icon_end - mouse_cursor_icon
