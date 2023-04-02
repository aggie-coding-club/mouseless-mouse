       
MEMORY
{
    ram(RW) : ORIGIN = 0, LENGTH = 512
}
SECTIONS
{
    .text : AT(12)
    {
        *(.text)
    } >ram
    .data :
    {
        . = ALIGN(4);
        *(.data)
    } >ram
    .bss :
    {
        . = ALIGN(4);
        *(.bss)
    } >ram
    .header : AT(0)
    {
        LONG(0x00706c75)
        SHORT(LOADADDR(.text))
        SHORT(SIZEOF(.text))
        SHORT(SIZEOF(.data))
        SHORT(SIZEOF(.bss))
    }
}
