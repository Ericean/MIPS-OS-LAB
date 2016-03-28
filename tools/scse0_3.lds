OUTPUT_ARCH(mips)
 
 
 
ENTRY(_start)
 
SECTIONS
 
{
 
. =0x80010000;
 
.text :{
 
   *(.int)
 
   *(.text)
 
   *(.fini)
 
  }

.bss :{*(.bss)}
.sdata :{*(.sdata)}
.data :{*(.data)}

. =0x80400000;

end =.;
 
}

