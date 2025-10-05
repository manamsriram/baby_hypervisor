#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

#define NUM_REGISTERS 32

// Register names mapping (index to $n notation)
const char *reg_names[NUM_REGISTERS] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

struct VMConfig {
    std::string assembly_file;      // config file for the VM
    std::string instruction_file;   // parsed from vm_binary=...
    std::string snapshot_file;      // parsed from vm_snapshot=...
    bool load_snapshot = false;     // true only if -s present for this VM
    int slice = 100;                // parsed from vm_exec_slice_in_instructions=...
};

// Find register index from $n or name
int reg_index(const std::string &r) {
    if (r.empty()) return -1;
    if (r[0] == '$' && r.length() > 1 && isdigit(r[1])) {
        try {
            return std::stoi(r.substr(1));
        } catch (...) { }
    }
    for (int i = 0; i < NUM_REGISTERS; ++i) {
        if (r == reg_names[i]) return i;
    }
    return -1;
}

// Print processor state for DUMP_PROCESSOR_STATE
void dump_processor_state(const int32_t reg[NUM_REGISTERS]) {
    for (int i = 1; i < NUM_REGISTERS; ++i) { // skip $zero
        std::cout << "R" << i << "=" << reg[i] << std::endl;
    }
}

// Save registers to a binary file
bool save_snapshot(const int32_t reg[NUM_REGISTERS], const std::string &snapfile) {
    std::ofstream out(snapfile, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(reg), NUM_REGISTERS * sizeof(int32_t));
    out.close();
    return true;
}

// Load registers from a binary file
bool load_snapshot(int32_t reg[NUM_REGISTERS], const std::string &snapfile) {
    std::ifstream in(snapfile, std::ios::binary);
    if (!in) return false;
    in.read(reinterpret_cast<char*>(reg), NUM_REGISTERS * sizeof(int32_t));
    in.close();
    return true;
}

// Parse a config/assembly file for VM setup
bool parse_vm_config(const std::string& fname, VMConfig& vm) {
    std::ifstream conf(fname);
    if (!conf) return false;
    std::string line;
    while (std::getline(conf, line)) {
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);

        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        if (line.find("vm_binary") != std::string::npos) {
            vm.instruction_file = line.substr(line.find('=') + 1);
            vm.instruction_file.erase(0, vm.instruction_file.find_first_not_of(" \t"));
        }
        if (line.find("vm_snapshot") != std::string::npos) {
            vm.snapshot_file = line.substr(line.find('=') + 1);
            vm.snapshot_file.erase(0, vm.snapshot_file.find_first_not_of(" \t"));
        }
        if (line.find("vm_exec_slice_in_instructions") != std::string::npos) {
            std::string val = line.substr(line.find('=') + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            vm.slice = std::stoi(val);
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    // Parse all -v <assembly_config_file> [-s] arguments
    std::vector<VMConfig> vms;
    for (int i = 1; i < argc;) {
        if (std::string(argv[i]) == "-v" && (i+1 < argc)) {
            VMConfig vm;
            vm.assembly_file = argv[i+1];
            if (!parse_vm_config(vm.assembly_file, vm)) {
                std::cerr << "Failed to open config/assembly file: " << vm.assembly_file << std::endl;
                return 1;
            }
            i += 2;
            // Only set load_snapshot if -s argument directly follows this -v pair
            if (i < argc && std::string(argv[i]) == "-s" && (i+1 < argc)) {
                vm.load_snapshot = true;
                vm.snapshot_file = argv[i+1]; // Assign snapshot filename from command line argument!
                i += 2;
            }
            vms.push_back(vm);
        } else {
            std::cerr << "Usage: myvmm -v <assembly_file_vm1> [-s <snapshot_file_vm1>] -v <assembly_file_vm2> ...\n";
            return 1;
        }
    }


    // Run each VM one by one
    for (size_t v = 0; v < vms.size(); ++v) {
        std::cout << "====== VM #" << (v+1) << " ======" << std::endl;
        const auto& vm = vms[v];
        int32_t reg[NUM_REGISTERS] = {0};
        int slice = vm.slice;

        // Only load snapshot if requested
        if (vm.load_snapshot && !vm.snapshot_file.empty()) {
            if (load_snapshot(reg, vm.snapshot_file)) {
                std::cout << "Loaded snapshot: " << vm.snapshot_file << std::endl;
            }
        }

        std::ifstream prog(vm.instruction_file);
        if (!prog) { std::cerr << "Cannot open instruction file: " << vm.instruction_file << std::endl; continue; }

        std::string line;
        int instr_count = 0;
        while (std::getline(prog, line)) {
            // Remove comments
            size_t comment = line.find('#');
            if (comment != std::string::npos) line = line.substr(0, comment);

            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = line.find_last_not_of(" \t\r\n");
            line = line.substr(first, last - first + 1);

            std::istringstream iss(line);
            std::string instr;
            iss >> instr;
            if(instr.empty()) continue;

            // Parse args
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

            // DUMP_PROCESSOR_STATE
            if (instr == "DUMP_PROCESSOR_STATE") {
                dump_processor_state(reg);
                continue;
            }

            // SNAPSHOT <filename>
            if (instr == "SNAPSHOT" && args.size() == 1) {
                if (save_snapshot(reg, args[0])) {
                    std::cout << "Snapshot saved to " << args[0] << std::endl;
                } else {
                    std::cerr << "Failed to save snapshot: " << args[0] << std::endl;
                }
                continue;
            }

            // All register writes except $zero (index 0)
            if (instr == "li" && args.size() == 2) {
                int idx = reg_index(args[0]);
                int val = std::stoi(args[1]);
                if (idx > 0) reg[idx] = val;
            } else if (instr == "add" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    reg[rd] = reg[rs] + reg[rt];
            } else if (instr == "sub" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    reg[rd] = reg[rs] - reg[rt];
            } else if (instr == "addi" && args.size() == 3) {
                int rt = reg_index(args[0]), rs = reg_index(args[1]);
                int imm = std::stoi(args[2]);
                if (rt > 0 && rs >= 0)
                    reg[rt] = reg[rs] + imm;
            } else if (instr == "mul" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    reg[rd] = reg[rs] * reg[rt];
            } else if (instr == "and" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    reg[rd] = reg[rs] & reg[rt];
            } else if (instr == "or" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int val;
                if (args[2][0] == '$')
                    val = reg[reg_index(args[2])];
                else
                    val = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    reg[rd] = reg[rs] | val;
            } else if (instr == "ori" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int val = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    reg[rd] = reg[rs] | val;
            } else if (instr == "xor" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]), rt = reg_index(args[2]);
                if (rd > 0 && rs >= 0 && rt >= 0)
                    reg[rd] = reg[rs] ^ reg[rt];
            } else if (instr == "sll" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int shamt = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    reg[rd] = reg[rs] << shamt;
            } else if (instr == "srl" && args.size() == 3) {
                int rd = reg_index(args[0]), rs = reg_index(args[1]);
                int shamt = std::stoi(args[2]);
                if (rd > 0 && rs >= 0)
                    reg[rd] = (uint32_t)reg[rs] >> shamt;
            }
            // else: unsupported instructions are skipped

            instr_count++;
            if (instr_count >= slice) break;
        }
        prog.close();
        // Optionally show final state
        //dump_processor_state(reg);
    }
    return 0;
}

