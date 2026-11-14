# boot/ - per-architecture boot loaders

    i386/    the working x86 port: stage1 (El Torito sector), stage2 (ELF32
             loader) and kernel.ld for the Linux link path.
    arm/     placeholder for the ARM port.
    amd64/   placeholder for the x86-64 port.
    mips/    placeholder for the MIPS port.
    riscv/   placeholder for the RISC-V port.

Only i386/ is wired into the build today: build_iso.ps1 / build_iso.sh
assemble boot/i386/stage1.s and boot/i386/stage2.s, and build_kernel.sh uses
boot/i386/kernel.ld.  The empty directories only carry a .keep file so the
layout survives git.
