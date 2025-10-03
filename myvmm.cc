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

int main(int argc, char *argv[]) {
    if (argc != 3 || std::string(argv[1]) != "-v") {
        std::cerr << "Usage: " << argv[0] << " -v <config_file>\n";
        return 1;
    }

    // Parse config file
    std::ifstream conf(argv[2]);
    if (!conf) { std::cerr << "Cannot open config file.\n"; return 1; }
    std::string asm_file;
    int slice = 100;
    std::string line;
    while (std::getline(conf, line)) {
        if (line.find("vm_binary") != std::string::npos) {
            asm_file = line.substr(line.find('=') + 1);
            asm_file.erase(0, asm_file.find_first_not_of(" \t"));
        }
        if (line.find("vm_exec_slice_in_instructions") != std::string::npos) {
            slice = std::stoi(line.substr(line.find('=') + 1));
        }
    }
    conf.close();

    // Processor registers (all zero)
    int32_t reg[NUM_REGISTERS] = {0};

    // Open asm/program file
    std::ifstream prog(asm_file);
    if (!prog) { std::cerr << "Cannot open asm file.\n"; return 1; }

    int instr_count = 0;
    while (std::getline(prog, line)) {
        // Remove comments
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);

        // Trim leading/trailing spaces
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue; // empty
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        std::istringstream iss(line);
        std::string instr;
        iss >> instr;
        if(instr.empty()) continue;

        // DUMP_PROCESSOR_STATE
        if (instr == "DUMP_PROCESSOR_STATE") {
            dump_processor_state(reg);
            continue;
        }

        // Parse the rest, comma separated args, blank-safe, strip whitespace
        std::vector<std::string> args;
        std::string token, arg;
        std::getline(iss, token);
        std::istringstream argss(token);
        while (std::getline(argss, arg, ',')) {
            // Trim arg
            size_t fa = arg.find_first_not_of(" \t\r\n");
            size_t la = arg.find_last_not_of(" \t\r\n");
            if (fa != std::string::npos)
                arg = arg.substr(fa, la - fa + 1);
            else
                arg.clear();
            if (!arg.empty()) args.push_back(arg);
        }

        // All register writes except $zero (index 0)
        if (instr == "li" && args.size() == 2) {
            int idx = reg_index(args[0]);
            int val = std::stoi(args[1]);
            if (idx > 0)
                reg[idx] = val;
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
    return 0;
}

