#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncurses.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <cstdlib>

int main(int argc, char *argv[])
{  
    if(argc!=2){
        printf("Usage: sudo ./vmm guest");
        return EXIT_FAILURE;
    }
    int kvm, vmfd, vcpufd, ret;
  

    uint8_t *mem;
    struct kvm_sregs sregs;
    size_t mmap_size;
    struct kvm_run *run;

//**initializing KVM and creating a VM

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1)
        err(1, "/dev/kvm");

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM_GET_API_VERSION");
    if (ret != 12)
        errx(1, "KVM_GET_API_VERSION %d, expected 12", ret);

    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0);
    if (vmfd == -1)
        err(1, "KVM_CREATE_VM");


//** loading binary code into  the allocated guest memory

    /* Allocate one aligned page of guest memory to hold the code. */
    mem = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!mem)
        err(1, "allocating guest memory");

FILE *file;
char *code_buffer;
unsigned long fileLen;

//Open file
file = fopen(argv[1], "rb");
if (!file)
{
fprintf(stderr, "Unable to open file %s", argv[1]);
        return EXIT_FAILURE;
}

//Get file length
fseek(file, 0, SEEK_END);
fileLen = ftell(file);
fseek(file, 0, SEEK_SET);

//Allocate memory
code_buffer = (char *)malloc(fileLen+1);
if (!code_buffer)
{
fprintf(stderr, "Memory error!");
        fclose(file);
        return EXIT_FAILURE;
}

//Read file contents into buffer
fread(code_buffer, fileLen, 1, file);
fclose(file);

    memcpy(mem, code_buffer, fileLen);

    /* Map it to the second page frame (to avoid the real-mode IDT at 0). */
    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .flags = KVM_MEM_LOG_DIRTY_PAGES,
        .guest_phys_addr = 0x1000,
        .memory_size = 0x10000,
        .userspace_addr = (uint64_t)mem,
    };

    //** Configuring virtual CPU  and the virtual machine's memory
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM_SET_USER_MEMORY_REGION");

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
    if (vcpufd == -1)
        err(1, "KVM_CREATE_VCPU");

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        errx(1, "KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    if (!run)
        err(1, "mmap vcpu");

    /* Initialize CS to point at 0, via a read-modify-write of sregs. */
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");

    /* Initialize registers: instruction pointer for our code, addends, and
     * initial flags required by x86 architecture. */
    struct kvm_regs regs = {
        .rip = 0x1000,
        .rax = 2,
        .rbx = 2,
        .rflags = 0x2,
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM_SET_REGS");

    char buffer[500];
    size_t current_size = 0;

    struct{
        bool status;
        char key;
    } keyboard = { .status = false };
    char c;

    struct{
        bool fire;
        bool enable;
        clock_t start_time;
        uint8_t interval;
        uint8_t port47;
    } timer = { .fire = false, .enable = false, .port47 = 0 };
    int time_diff;


    /* Initialize ncurses to immediately make the charater available without waiting for user input */
    initscr();
    resize_term(25,80);
    cbreak();
    nodelay(stdscr,TRUE);
    noecho();

//**Executing the VM and handling different exit scenarios

    /* Repeatedly run code and handle VM exits. */
    while (1) {
        if(timer.enable){
            time_diff = (clock() - timer.start_time)*1000/CLOCKS_PER_SEC;
            if(time_diff >= timer.interval){
                timer.port47 |= 1UL << 1;
                timer.start_time = clock();
            }
        }

        c = getch();
        if(c!=ERR){
            keyboard.key = c;
            keyboard.status = true;
        }

        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1)
            err(1, "KVM_RUN");
        switch (run->exit_reason) {
        case KVM_EXIT_HLT:
            puts("KVM_EXIT_HLT");
            return 0;
        case KVM_EXIT_IO:
            if (run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x42 && run->io.count == 1){
                buffer[current_size++] = *(((char *)run) + run->io.data_offset);
                if (buffer[current_size-1] == '\n'){
                    buffer[current_size++] = '\0';
                    for(size_t i=0;i<current_size;i++){
                    if(!isprint(buffer[i]))
                    buffer[i]='';
                }
		   // clear();
                    printf("%s",buffer);
                    fflush(stdout);
                    usleep(100000);
		   // refresh();
		            move(0,0);
                    memset(buffer, 0, 500);
                    current_size = 0;
                }
            }

            else if(run->io.direction == KVM_EXIT_IO_IN && run->io.size == 1 && run->io.port == 0x45 && run->io.count == 1){
                *(((uint8_t *)run) + run->io.data_offset) = keyboard.status;
                
            }

            else if(run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x45 && run->io.count == 1){
                keyboard.status = *(((uint8_t *)run) + run->io.data_offset);
            }

            else if(run->io.direction == KVM_EXIT_IO_IN && run->io.size == 1 && run->io.port == 0x44 && run->io.count == 1){
                *(((uint8_t *)run) + run->io.data_offset) = keyboard.key;
            }

            else if(run->io.direction == KVM_EXIT_IO_IN && run->io.size == 1 && run->io.port == 0x47 && run->io.count == 1){
                *(((uint8_t *)run) + run->io.data_offset) = timer.port47;
            }

            else if(run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x47 && run->io.count == 1){
                timer.port47 = *(((uint8_t *)run) + run->io.data_offset);
                if(!timer.enable){
                    timer.enable = (timer.port47 & 1U);
                    if(timer.enable) timer.start_time = clock();
                } else{
                    timer.enable = (timer.port47 & 1U);
                }
               
            }

            else if(run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x46 && run->io.count == 1){
                timer.interval = *(((uint8_t *)run) + run->io.data_offset);
            }
            else
                errx(1, "unhandled KVM_EXIT_IO");
            break;
        case KVM_EXIT_FAIL_ENTRY:
            errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                 (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
        case KVM_EXIT_INTERNAL_ERROR:
            errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", run->internal.suberror);
        case KVM_EXIT_MMIO:
        printf("Got an unexpected MMIO exit:"
        " phys_addr %#llx,"
        " data %02x %02x %02x %02x"
        " %02x %02x %02x %02x,"
        " len %u, is_write %hhu",
        (unsigned long long) run->mmio.phys_addr,
        run->mmio.data[0], run->mmio.data[1],
        run->mmio.data[2], run->mmio.data[3],
        run->mmio.data[4], run->mmio.data[5],
        run->mmio.data[6], run->mmio.data[7],
        run->mmio.len, run->mmio.is_write);

        default:
            errx(1, "exit_reason = 0x%x", run->exit_reason);
        }
    }
}


