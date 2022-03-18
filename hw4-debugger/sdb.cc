#include "sdb.hpp"

#include <capstone/capstone.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "utils.hpp"

using namespace std;

Sdb::Sdb() {
    command_functions["exit"] = command_functions["q"] = [this](vector<string> args) { exit(0); };

    command_functions["help"] = command_functions["h"] = lambda_wrapper(help);
    command_functions["load"] = lambda_wrapper(load);
    command_functions["start"] = lambda_wrapper(start);
    command_functions["run"] = command_functions["r"] = lambda_wrapper(run);
    command_functions["set"] = command_functions["s"] = lambda_wrapper(set_reg);
    command_functions["get"] = command_functions["g"] = lambda_wrapper(get_reg);
    command_functions["getregs"] = lambda_wrapper(dump_regs);
    command_functions["dump"] = command_functions["x"] = lambda_wrapper(dump_mem);
    command_functions["disasm"] = command_functions["d"] = lambda_wrapper(disasm);
    command_functions["vmmap"] = command_functions["m"] = lambda_wrapper(vmmap);

    command_functions["break"] = command_functions["b"] = lambda_wrapper(set_bp);
    command_functions["delete"] = lambda_wrapper(remove_bp);
    command_functions["list"] = command_functions["l"] = lambda_wrapper(list_bp);
    command_functions["si"] = lambda_wrapper(single_step);
    command_functions["cont"] = command_functions["c"] = lambda_wrapper(cont);
}

Sdb::~Sdb() {
}

map<string, ull *> Sdb::fetch_regs() {
    ptrace(PTRACE_GETREGS, this->pid, 0, &regs_struct);

    map<string, ull *> regs;
    regs["rax"] = &regs_struct.rax;
    regs["rbx"] = &regs_struct.rbx;
    regs["rcx"] = &regs_struct.rcx;
    regs["rdx"] = &regs_struct.rdx;
    regs["rsp"] = &regs_struct.rsp;
    regs["rbp"] = &regs_struct.rbp;
    regs["rsi"] = &regs_struct.rsi;
    regs["rdi"] = &regs_struct.rdi;
    regs["rip"] = &regs_struct.rip;
    regs["r8"] = &regs_struct.r8;
    regs["r9"] = &regs_struct.r9;
    regs["r10"] = &regs_struct.r10;
    regs["r11"] = &regs_struct.r11;
    regs["r12"] = &regs_struct.r12;
    regs["r13"] = &regs_struct.r13;
    regs["r14"] = &regs_struct.r14;
    regs["r15"] = &regs_struct.r15;
    regs["flags"] = &regs_struct.eflags;

    return regs;
}

long Sdb::get_code(ull address) {
    return ptrace(PTRACE_PEEKTEXT, this->pid, address, 0);
}

void Sdb::check_bp() {
    int status;
    waitpid(pid, &status, 0);

    if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) != SIGTRAP) {
            cerr << "** child process " << pid << " stopped by signal (code " << WSTOPSIG(status) << ")" << endl;
            return;
        }

        auto regs = fetch_regs();
        *(regs["rip"]) -= 1;
        long _code = get_code(*regs["rip"]);
        unsigned char *code = ((unsigned char *)&_code);
        if (*code == 0xcc) {
            auto it = bp_addr_map.find(*regs["rip"]);
            if (it == bp_addr_map.end()) return;

            hit_bp = true;
            *code = it->second.original;
            cerr << "** breakpoint @ " << hex;

            // <-- disasm
            csh handle;
            if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
                return;

            cs_insn *insn;
            size_t count = cs_disasm(handle, (uint8_t *)&_code, sizeof(_code) - 1, *regs["rip"], 1, &insn);

            cerr << setw(8) << right << setfill(' ') << insn[0].address << ": " << setfill(' ');
            stringstream hex_bytes;
            for (int i = 0; i < insn[0].size; ++i) {
                hex_bytes << setw(2) << right << setfill('0')
                          << hex << (unsigned int)insn[0].bytes[i] << " ";
            }
            cerr << setw(24) << left << hex_bytes.str()
                 << setw(5) << left << insn[0].mnemonic << " " << insn[0].op_str
                 << dec << endl;
            cs_free(insn, count);
            cs_close(&handle);
            ptrace(PTRACE_SETREGS, pid, NULL, &regs_struct);
            // disasm done -->
        }
    }

    if (WIFEXITED(status)) {
        printf("** child process %d terminiated normally (code %d)\n", this->pid, status);
    }
}

void Sdb::run(vector<string> args) {
    if (state == SdbState::loaded) start(args);
    if (state == SdbState::running) {
        cout << "** program " << this->program << " is already running." << endl;
        cont(args);
    } 
    cerr << "** Error: This command is for loaded or running program only." << endl;
}

void Sdb::set_bp(vector<string> args) {
    state_constraint(SdbState::running);
    ull address = to_uul(args[1]);

    auto code = ptrace(PTRACE_PEEKTEXT, this->pid, address, NULL);

    Breakpoint bp = {this->bpid++, address, (unsigned char)code};
    bp_addr_map[bp.address] = bp_id_map[bp.id] = bp;
    ptrace(PTRACE_POKETEXT, pid, address, (code & 0xffffffffffffff00) | 0xcc);
}

void Sdb::remove_bp(vector<string> args) {
    state_constraint(SdbState::running);
    int id = to_uul(args[1]);

    auto it = bp_id_map.find(id);
    if (it != bp_id_map.end()) {
        Breakpoint bp = it->second;
        auto code = ptrace(PTRACE_PEEKTEXT, this->pid, bp.address, NULL);
        ptrace(PTRACE_POKETEXT, pid,
               bp.address, (code & 0xffffffffffffff00) | bp.original);
        bp_id_map.erase(it);
        cerr << "** breakpoint " << id << " deleted." << endl;
        return;
    }

    cerr << "** No such breakpoint id." << endl;
}

void Sdb::list_bp(vector<string> args) {
    state_constraint(SdbState::running);
    for (auto [id, bp] : this->bp_id_map)
        cerr << id << ": " << hex << bp.address << dec << endl;
}

void Sdb::single_step(vector<string> args) {
    state_constraint(SdbState::running);
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
}

void Sdb::cont(vector<string> args) {
    state_constraint(SdbState::running);
    ptrace(PTRACE_CONT, pid, NULL, NULL);
    check_bp();
}

void Sdb::dump_mem(vector<string> args) {
    state_constraint(SdbState::running);
    ull address = to_uul(args[1]);
    size_t size = args.size() > 2 ? to_uul(args[2]) : 80;

    for (int i = 0; i < (size / 16) + (size % 16 != 0); i++) {
        string memory = "";
        for (int j = 0; j < 2; j++) {
            auto res = ptrace(PTRACE_PEEKTEXT, pid, address, NULL);
            memory += string((char *)&res, 8);
            address += 8;
        }
        cerr << "\t" << hex << address - 16 << ": ";
        for (auto ch : memory)
            cerr << setfill('0') << setw(2) << +((unsigned char)ch) << " ";
        cerr << "|";
        for (auto ch : memory)
            cerr << (!isprint(ch) ? '.' : ch);
        cerr << "|" << dec << endl;
    }
}

void Sdb::set_reg(vector<string> args) {
    state_constraint(SdbState::running);
    string reg_name = args[1], value = args[2];
    auto regs = fetch_regs();

    *regs[args[1]] = to_uul(args[2]);
    ptrace(PTRACE_SETREGS, pid, NULL, &regs_struct);
}

void Sdb::get_reg(vector<string> args) {
    state_constraint(SdbState::running);
    auto regs = fetch_regs();
    string reg_name = args[1];
    if (regs.count(reg_name)) {
        cerr << "$" << reg_name << " = " << *regs[reg_name]
             << hex << " (0x" << *regs[reg_name] << ")" << dec << endl;
        return;
    }
    cerr << "** No such register." << endl;
}

void Sdb::dump_regs(vector<string> args) {
    state_constraint(SdbState::running);
    for (auto [reg_name, val] : fetch_regs()) {
        cerr << "$" << reg_name << " = " << *val
             << hex << " (0x" << *val << ")" << dec << endl;
    }
}

void Sdb::disasm(vector<string> args) {
    state_constraint(SdbState::running);
    ull address = to_uul(args[1]);
    long code_segments[10];
    for (int i = 0; i < 10; i++) {
        ull target_addr = (address + i * 8);
        code_segments[i] = this->get_code(target_addr);

        // put original opcode back.
        for (int j = 0; j < sizeof(long); j++) {
            unsigned char *code = &((unsigned char *)&code_segments[i])[j];
            if (*code == 0xcc) {
                auto it = this->bp_addr_map.find(address + i * 8 + j);
                if (it == bp_addr_map.end()) continue;
                *code = it->second.original;
            }
        }
    }

    csh handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return;
    cs_insn *insn;
    size_t count = cs_disasm(handle, (uint8_t *)code_segments, sizeof(code_segments) - 1, address, 10, &insn);
    if (count > 0) {
        cerr << hex;
        for (int i = 0; i < count; ++i) {
            cerr << setw(12) << right << setfill(' ') << insn[i].address << ": " << setfill(' ');
            stringstream hex_bytes;
            for (int j = 0; j < insn[i].size; ++j) {
                hex_bytes << setw(2) << right << setfill('0')
                          << hex << (unsigned int)insn[i].bytes[j] << " ";
            }
            cerr << setw(24) << left << hex_bytes.str()
                 << setw(5) << left << insn[i].mnemonic << " " << insn[i].op_str << endl;
        }
        cerr << dec;
        cs_free(insn, count);
    }
    cs_close(&handle);
}

void Sdb::load(vector<string> args) {
    if (state != SdbState::init) {
        cerr << "** Program has been loaded." << endl;
        cerr << "** Entry address = 0x" << hex << this->entry_address << dec << endl;
        return;
    }
    string program = args[1];

    struct stat st;
    if (stat(program.c_str(), &st) >= 0 &&
        (st.st_mode & S_IEXEC) != 0 && (st.st_mode & S_IFREG) != 0) {
        this->program = program;
    }

    if ((this->pid = fork()) < 0) {
        return;
    } else if (this->pid == 0) {  // child
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) die("traceme");
        execlp(program.c_str(), program.c_str(), NULL);
        die("exec failed");
    }

    // parent
    int pid_status;
    waitpid(pid, &pid_status, 0);
    ptrace(PTRACE_SETOPTIONS, this->pid, 0, PTRACE_O_EXITKILL);

    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, this->pid, 0, &regs);
    this->entry_address = regs.rip;
    cerr << "** Program `" << program << "` loaded. Entry address = 0x" << hex << regs.rip << dec << endl;

    this->state = SdbState::loaded;
}

void Sdb::start(vector<string> args) {
    state_constraint(SdbState::loaded);
    cerr << "** Starting program: " << program << endl;
    cerr << "** pid " << this->pid << endl;

    this->state = SdbState::running;
}

void Sdb::help(vector<string> args) {
    cerr << R"(- break {instruction-address}: add a break point
- cont: continue execution
- delete {break-point-id}: remove a break point
- disasm addr: disassemble instructions in a file or a memory region
- dump addr [length]: dump memory content
- exit: terminate the debugger
- get reg: get a single value from a register
- getregs: show registers
- help: show this message
- list: list break points
- load {path/to/a/program}: load a program
- run: run the program
- vmmap: show memory layout
- set reg val: get a single value to a register
- si: step into instruction
- start: start the program and stop at the first instruction
)";
}

void Sdb::vmmap(vector<string>) {
    state_constraint(SdbState::running);
    ifstream maps_file("/proc/" + to_string(this->pid) + "/maps");
    string line;
    while (getline(maps_file, line)) cerr << line << endl;
}

void Sdb::launch(string program = "", string script_path = "") {
    ifstream script_in;
    bool load_from_script = false;
    if (script_path != "") {
        script_in = ifstream(script_path);
        if (script_in.fail()) {
            cerr << "** Load from script `" << script_path << "` failed." << endl;
            return;
        }
        load_from_script = true;
        cerr << "** Load from script `" << script_path << "` successfully." << endl;
    }
    if (program != "") {
        std::vector<string> args = {"load", program};
        this->load(args);
    }

    while (true) {
        string in_command;
        if (!load_from_script) cerr << "sdb> ";
        getline(load_from_script ? script_in : cin, in_command);
        if (cin.eof() || script_in.eof()) break;
        vector<string> args = tokenizer(in_command);
        if (args.size() == 0) continue;
        string command = args[0];
        // cerr << "** Command: " << command << endl;
        auto cmd_func = command_functions.find(command);
        if (cmd_func != command_functions.end())
            cmd_func->second(args);
        else
            cerr << "Undefined command: \"" << command << "\".  Try \"help\"." << endl;
    }
}
