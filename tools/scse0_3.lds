OUTPUT_ARCH(mips)
 
 
 
ENTRY(_start)
 
SECTIONS
 
{
 
. =0x80200000;
 
.text :{
 
   *(.int)
 
   *(.text)
 
   *(.fini)
 
  }
 
. =0x80400000;
 
.data :{*(.data)}
 
.bss :{*(.bss)}
end =.;
 
}

