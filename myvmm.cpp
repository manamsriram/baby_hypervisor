#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

// ADD THESE HEADERS FOR NETWORKING
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Configurable number of registers
#define NUM_REGISTERS 32
const char *reg_names[NUM_REGISTERS] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

struct VMConfig {
    std::string assembly_file;
    std::string instruction_file;
    std::string snapshot_file;
    bool load_snapshot = false;
    int slice = 100;
};

// Full processor state (registers + PC)
struct VMState {
    int32_t reg[NUM_REGISTERS];
    uint32_t pc;
};

// Networking function: waits for migration transfer and saves to file
void receive_snapshot(int port, const std::string& output_file) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 1);

    std::cout << "[migration] Waiting for VM state on port " << port << "\n";
    int client_socket = accept(server_fd, nullptr, nullptr);
    std::ofstream fout(output_file, std::ios::binary);
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(client_socket, buffer, sizeof(buffer))) > 0) {
        fout.write(buffer, bytes);
    }
    fout.close();
    close(client_socket);
    close(server_fd);
    std::cout << "[migration] Snapshot received as " << output_file << "\n";
}

// Networking function: sends a snapshot to given ip:port
void send_snapshot(const std::string& ip, int port, const std::string& snapshot_file) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    std::cout << "[migration] Connecting to " << ip << ":" << port << "\n";
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[migration] Connection failed!\n";
        return;
    }
    std::ifstream fin(snapshot_file, std::ios::binary);
    char buffer[4096];
    while (fin.read(buffer, sizeof(buffer)) || fin.gcount() > 0) {
        send(sock, buffer, fin.gcount(), 0);
    }
    fin.close();
    close(sock);
    std::cout << "[migration] Migration sent!\n";
}

// Register name to index
int reg_index(const std::string &r) {
    if (r.empty()) return -1;
    if (r[0] == '$' && r.length() > 1 && isdigit(r[1])) {
        try { return std::stoi(r.substr(1)); } catch (...) {}
    }
    for (int i = 0; i < NUM_REGISTERS; ++i)
        if (r == reg_names[i]) return i;
    return -1;
}

void dump_processor_state(const int32_t reg[NUM_REGISTERS]) {
    for (int i = 0; i < NUM_REGISTERS; ++i)
        std::cout << "$r" << i << "=" << reg[i] << std::endl;
}

bool save_snapshot(const VMState& state, const std::string& snapfile) {
    std::ofstream out(snapfile, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&state), sizeof(VMState));
    out.close();
    return true;
}

bool load_snapshot(VMState& state, const std::string& snapfile) {
    std::ifstream in(snapfile, std::ios::binary);
    if (!in) return false;
    in.read(reinterpret_cast<char*>(&state), sizeof(VMState));
    in.close();
    return true;
}

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
    std::vector<VMConfig> vms;
    VMConfig pending;

    // SUPPORTS: -v <file> -s <snapshot> -p <port> 
    int migration_port = 0;
    for (int i = 1; i < argc;) {
        if (std::string(argv[i]) == "-v" && (i + 1) < argc) {
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
            pending.snapshot_file = argv[i+1];
            pending.load_snapshot = true;
            i += 2;
        } else if (std::string(argv[i]) == "-p" && (i + 1) < argc) {
            migration_port = std::stoi(argv[i+1]);
            i += 2;
        } else {
            std::cerr << "Usage: myvmm [-s SNAPSHOT] -v ASSYFILE [-p PORT]\n";
            return 1;
        }
    }
    if (!pending.assembly_file.empty()) vms.push_back(pending);

    // If -p (receiver mode): Listen for incoming migration before resuming VM
    if (migration_port > 0) {
        receive_snapshot(migration_port, "migrated_snapshot.bin");
        if (vms.size() > 0) {
            vms[0].snapshot_file = "migrated_snapshot.bin";
            vms[0].load_snapshot = true;
        }
    }

    for (size_t v = 0; v < vms.size(); ++v) {
        std::cout << "====== VM #" << (v+1) << " ======" << std::endl;
        const auto& vm = vms[v];
        VMState state = {}; // Clear processor state

        int slice = vm.slice;
        if (vm.load_snapshot && !vm.snapshot_file.empty()) {
            if (load_snapshot(state, vm.snapshot_file)) {
                std::cout << "Loaded snapshot: " << vm.snapshot_file << std::endl;
            } else {
                std::cerr << "Failed to load snapshot: " << vm.snapshot_file << std::endl;
            }
        }

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

        uint32_t pc = state.pc;
        int instr_count = 0;
        while (pc < instructions.size() && instr_count < slice) {
            std::cout << "Executing PC=" << pc << ": " << instructions[pc] << std::endl;
            std::istringstream iss(instructions[pc]);
            std::string instr;
            iss >> instr;
            if (instr.empty()) { pc++; continue; }
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
            // MIGRATION HANDLER
            if (instr == "MIGRATE" && args.size() == 1) {
                size_t colon = args[0].find(':');
                std::string ip = args[0].substr(0, colon);
                int port = std::stoi(args[0].substr(colon+1));
                std::string snapshot_file = "temp_migrate_snapshot.bin";
                state.pc = pc + 1;
                if (!save_snapshot(state, snapshot_file)) {
                    std::cerr << "[migration] Could not create snapshot!\n";
                    break;
                }
                send_snapshot(ip, port, snapshot_file);
                std::cout << "[migration] VM migrated; stopping local execution.\n";
                break; // Optional: stop VM after migration, or remove this line if you want to keep running
            }
            // SNAPSHOT
            else if (instr == "SNAPSHOT" && args.size() == 1) {
                state.pc = pc + 1;
                if (save_snapshot(state, args[0])) {
                    std::cout << "Snapshot saved to " << args[0] << std::endl;
                } else {
                    std::cerr << "Failed to save snapshot: " << args[0] << std::endl;
                }
            }
            // Arithmetic and other handlers...
            else if (instr == "DUMP_PROCESSOR_STATE") {
                dump_processor_state(state.reg);
            }
            else if (instr == "li" && args.size() == 2) {
                int idx = reg_index(args[0]);
                int val = std::stoi(args[1]);
                if (idx > 0) state.reg[idx] = val;
            } 
            else if (instr == "add" && args.size() == 3) {
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
