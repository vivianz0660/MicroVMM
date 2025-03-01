# Micro VMM
A custom hypervisor using the Linux KVM API

# Requirements
- Target Architecture: Designed for x86-64, running on Linux using KVM.
- Binary File: Accept a binary file as a command-line argument, load it into guest memory, and run it.
- Console Device: Implement a simple line-buffered console that saves bytes to a buffer and prints it when a newline is written to IO port 0x42.
- Virtual Keyboard Device: Accept input from STDIN, set corresponding byte in IO port 0x44, and notify the guest via IO port 0x45.
- Virtual Interval Timer: The guest writes a time value to IO port 0x46, and the timer notifies the guest of events by setting bit 1 in IO port 0x47.
- Initial Guest: The guest starts as a simple binary but must include console and keyboard drivers to interact with virtual devices.
- Polled I/O: Use polled I/O instead of interrupts. The guest checks device status registers in an event loop.
- Event Handling: The guest echoes input to the console after the user presses enter, on every timer event, until a new line is entered.

# To run program:
- ~> make
- ~> sudo ./basic-vmm smallkern

# Prerequisites 
- ncurses
- nasm
