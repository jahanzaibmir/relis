# Relis

RELIS stands for Raw Elemental Low level Instruction System. It is a 64 bit x86_64 kernel. The main goal of the project is to explore the bare metal foundations of system programming. Bypassing modern software layers, it starts at the absolute base level to manage memory, handle hardware interrupts, and talk directly to the processor. 

------------------------------------------------------------------------------------------------------------

## DISCLAIMER
#### This project is incomplete and contains critical multiprocessing bugs
#### The bugs are in every file but the fatal bug in waking the AP is letting me turn this project offline
#### I am stepping away from development after exhausting available time
#### but encourage anyone interested to fork the repository and contribute.

-------------------------------------------------------------------------------------------------------------
#### The core issues preventing Application Processor 'AP' startup are documented below:

Bug 1 (trampoline.asm): Paging (CR0.PG) is enabled without setting Protection Enable (CR0.PE), causing an immediate #GP triple-fault in Real Mode.

Bug 2 (trampoline.asm): Uninitialized Real Mode segment registers (DS, ES, SS) cause memory accesses to read from incorrect garbage physical addresses.

Bug 3 (smp_boot.c): The trampoline code is copied to physical 0x108000 instead of physical 0x8000 due to a direct map offset mismatch.

Bug 4 (apic.c): The BSP hangs forever polling bit 12 of LAPIC_ICR_LO, which is reserved and permanently reads as 1 on modern x86/QEMU.

Bug 5 (smp_boot.c): Enabled interrupts allow timer IRQs to preempt the BSP during the INIT-SIPI sequence, breaking the required timing delay. ***


---------------------------------------------------------------------------------------------------------------

##### You can compile the project with make and make run 
##### Make sure to have nasm masm binutils xorriso qemu and ld
