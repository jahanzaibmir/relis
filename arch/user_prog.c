void _start(void) {
    // Just spin safely to prove Ring 3 execution is stable.
    while(1) { __asm__ volatile("pause"); }
}
