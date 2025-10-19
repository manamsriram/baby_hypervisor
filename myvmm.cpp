#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

#define NUM_REGISTERS 32

// Mapping a register index to its symbolic MIPS name for reference/printing
const char *reg_names[NUM_REGISTERS] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

// Holds all configuration data for a single VM instance
struct VMConfig {
    std::string assembly_file;    // The VM's config file path
    std::string instruction_file; // Parsed from vm_binary=... in config/assembly
    std::string snapshot_file;    // Snapshot file associated with this VM (from config or CLI)
    bool load_snapshot = false;   // True if snapshot should be loaded on startup
    int slice = 100;              // Number of instructions to execute (from config, default 100)
};

// Holds the runtime VM state to be saved/restored in snapshots
struct VMState {
    int32_t reg[NUM_REGISTERS];   // All 32 integer registers
    uint32_t pc;                  // The program counter (next instruction to execute)
};

// Returns register index for a given string ($n or symbolic name like $a0)
int reg_index(const std::string &r) {
    if (r.empty()) return -1;
    // Check for $n format (e.g. $4)
    if (r[0] == '$' && r.length() > 1 && isdigit(r[1])) {
        try { return std::stoi(r.substr(1)); } catch (...) {}
    }
    // Check for symbolic MIPS name (e.g. $s5)
    for (int i = 0; i < NUM_REGISTERS; ++i)
        if (r == reg_names[i]) return i;
    return -1;
}

// Print all 32 registers ($r0 to $r31). Use reg_names if you want symbolic names.
void dump_processor_state(const int32_t reg[NUM_REGISTERS]) {
    for (int i = 0; i < NUM_REGISTERS; ++i)
        std::cout << "$r" << i << "=" << reg[i] << std::endl;
    // Use reg_names[i] if you want symbolic names
    // std::cout << reg_names[i] << "=" << reg[i] << std::endl;
}

// Save VM state (registers + PC) to a binary snapshot file
bool save_snapshot(const VMState& state, const std::string& snapfile) {
    std::ofstream out(snapfile, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&state), sizeof(VMState));
    out.close();
    return true;
}

// Restore VM state (registers + PC) from binary snapshot file
bool load_snapshot(VMState& state, const std::string& snapfile) {
    std::ifstream in(snapfile, std::ios::binary);
    if (!in) return false;
    in.read(reinterpret_cast<char*>(&state), sizeof(VMState));
    in.close();
    return true;
}

// Parse the VM config/assembly file for keys like vm_binary, vm_snapshot, vm_exec_slice_in_instructions
bool parse_vm_config(const std::string& fname, VMConfig& vm) {
    std::ifstream conf(fname);
    if (!conf) return false;
    std::string line;
    while (std::getline(conf, line)) {
        // Remove comments
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        // Parse vm_binary setting: sets instruction file to run on this VM
        if (line.find("vm_binary") != std::string::npos) {
            vm.instruction_file = line.substr(line.find('=') + 1);
            vm.instruction_file.erase(0, vm.instruction_file.find_first_not_of(" \t"));
        }
        // Parse vm_snapshot: sets default snapshot if present in config
        if (line.find("vm_snapshot") != std::string::npos) {
            vm.snapshot_file = line.substr(line.find('=') + 1);
            vm.snapshot_file.erase(0, vm.snapshot_file.find_first_not_of(" \t"));
            // You could set vm.load_snapshot = true here if you want to always load from config snapshot
        }
        // Parse how many instructions to execute before halting
        if (line.find("vm_exec_slice_in_instructions") != std::string::npos) {
            std::string val = line.substr(line.find('=') + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            vm.slice = std::stoi(val);
        }
    }
    return true;
}

// Entry point: parses CLI arguments, sets up config per VM, runs each VM one at a time
int main(int argc, char *argv[]) {
    std::vector<VMConfig> vms;    // List of all VMs to run this session
    VMConfig pending;             // Will hold info for the VM currently being parsed

    // Parse CLI arguments. Supports mixed -v and -s order, associates snapshot with most recent VM
    for (int i = 1; i < argc;) {
        if (std::string(argv[i]) == "-v" && (i + 1) < argc) {
            // If previous VM spec is complete, push it to list
            if (!pending.assembly_file.empty()) {
                vms.push_back(pending);
                pending = VMConfig{};
            }
            pending.assembly_file = argv[i+1];
            if (!parse_vm_config(pending.assembly_file, pending)) {
                std::cerr << "Failed to open config/assembly file: " << pending.assembly_file << std::endl;
                return 1;
            }
            i += 2;
        } else if (std::string(argv[i]) == "-s" && (i + 1) < argc) {
            // Associate this snapshot with current pending VM (overrides config snapshot)
            pending.snapshot_file = argv[i+1];
            pending.load_snapshot = true;
            i += 2;
        } else {
            std::cerr << "Usage: myvmm [-s <snapshot_file>] -v <config_file> [...]" << std::endl;
            return 1;
        }
    }
    // Push last VM on CLI (if any) to VM set
    if (!pending.assembly_file.empty()) vms.push_back(pending);

    // Run each VM one by one (could extend to parallel execution if desired)
    for (size_t v = 0; v < vms.size(); ++v) {
        std::cout << "====== VM #" << (v + 1) << " ======" << std::endl;
        const auto& vm = vms[v];
        VMState state = {};      // Reset registers and PC to default for each VM
        int slice = vm.slice;    // How many instructions to execute, from config/CLI

        // If requested, load snapshot file for initial VM state
        if (vm.load_snapshot && !vm.snapshot_file.empty()) {
            if (load_snapshot(state, vm.snapshot_file)) {
                std::cout << "Loaded snapshot: " << vm.snapshot_file << std::endl;
            } else {
                std::cerr << "Failed to load snapshot: " << vm.snapshot_file << std::endl;
            }
        }
        
        // Load assembly/instruction file into memory for indexed access
        std::ifstream prog(vm.instruction_file);
        if (!prog) {
            std::cerr << "Cannot open instruction file: " << vm.instruction_file << std::endl;
            continue;
        }
        std::vector<std::string> instructions;
        std::string line;
        while (std::getline(prog, line)) {
            size_t comment = line.find('#');
            if (comment != std::string::npos) line = line.substr(0, comment);
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = line.find_last_not_of(" \t\r\n");
            line = line.substr(first, last - first + 1);
            if (!line.empty()) instructions.push_back(line);
        }
        prog.close();

        uint32_t pc = state.pc;  // If restoring, start at saved PC; else start at 0
        if(vm.load_snapshot) {
            std::cout << "Loaded snapshot. Resuming at instruction #" << state.pc << " / total instructions = " << instructions.size() << std::endl;
        }
        int instr_count = 0;
        while (pc < instructions.size() && instr_count < slice) {
            // Optional debug: show instruction being executed
            std::cout << "Executing PC=" << pc << ": " << instructions[pc] << std::endl;

            std::istringstream iss(instructions[pc]);
            std::string instr;
            iss >> instr;
            if (instr.empty()) { pc++; continue; }

            // Parse argument list from instruction line
            std::vector<std::string> args;
            std::string token, arg;
            std::getline(iss, token);
            std::istringstream argss(token);
            while (std::getline(argss, arg, ',')) {
                size_t fa = arg.find_first_not_of(" \t\r\n");
                size_t la = arg.find_last_not_of(" \t\r\n");
                if (fa != std::string::npos)
                    arg = arg.substr(fa, la - fa + 1);
                else
                    arg.clear();
                if (!arg.empty()) args.push_back(arg);
            }

            // Instruction handlers (add more for your ISA as needed)
            if (instr == "DUMP_PROCESSOR_STATE") {
                dump_processor_state(state.reg);
            }
            else if (instr == "SNAPSHOT" && args.size() == 1) {
                // Save state after this instruction, so next run starts at pc+1
                state.pc = pc+1;
                if (save_snapshot(state, args[0])) {
                    std::cout << "Snapshot saved to " << args[0] << std::endl;
                } else {
                    std::cerr << "Failed to save snapshot: " << args[0] << std::endl;
                }
            }
            // Arithmetic, logic, and register instructions (MIPS-style examples)
            else if (instr == "li" && args.size() == 2) {
                int idx = reg_index(args[0]);
                int val = std::stoi(args[1]);
                if (idx > 0) state.reg[idx] = val; // MIPS $zero ($r0) is always 0
            } else if (instr == "add" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    state.reg[rd] = state.reg[rs] + state.reg[rt];
            } else if (instr == "sub" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    state.reg[rd] = state.reg[rs] - state.reg[rt];
            } else if (instr == "addi" && args.size() == 3) {
                int rt = reg_index(args[0]), rs = reg_index(args[1]);
                int imm = std::stoi(args[2]);
                if (rt > 0 && rs >= 0)
                    state.reg[rt] = state.reg[rs] + imm;
            } else if (instr == "mul" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    state.reg[rd] = state.reg[rs] * state.reg[rt];
            } else if (instr == "and" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    state.reg[rd] = state.reg[rs] & state.reg[rt];
            } else if (instr == "or" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int val;
                if (args[2][0] == '$')
                    val = state.reg[reg_index(args[2])];
                else
                    val = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    state.reg[rd] = state.reg[rs] | val;
            } else if (instr == "ori" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int val = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    state.reg[rd] = state.reg[rs] | val;
            } else if (instr == "xor" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    state.reg[rd] = state.reg[rs] ^ state.reg[rt];
            } else if (instr == "sll" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int shamt = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    state.reg[rd] = state.reg[rs] << shamt;
            } else if (instr == "srl" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int shamt = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    state.reg[rd] = (uint32_t)state.reg[rs] >> shamt;
            }
            // Advance to next instruction
            pc++;
            instr_count++;
        }
    }
    return 0;
}

