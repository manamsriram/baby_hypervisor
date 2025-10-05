# 🧑‍💻 baby_hypervisor
**A Learning Hypervisor Project — UTM Virtualization, C++ Backed, Stepwise Innovation**

---

## 🚀 Overview

baby_hypervisor is a personal exploration into **hypervisor development using C++**, designed to virtualize a custom Ubuntu server image atop the [UTM](https://mac.getutm.app/) hypervisor platform. The project builds incrementally, aiming to demystify how low-level virtualization works, and brings new capabilities with each release.

## 💡 Current Features (v0.2)

- **Arithmetic, Logical & Numeric Operations:**  
  The initial implementation enables basic VM operation decoding for fundamental arithmetic, logical, and numeric tasks inside the guest Ubuntu instance.

- **UTM Integration:**  
  Ubuntu is deployed and managed as a VM using UTM, enabling rapid iteration and robust snapshotting for experimental development.

- **Modern C++ Foundations:**  
  VM core and opcode interpreter written in C++, with extensible design for isolation, error handling, and future expansion.

- **Snapshot Support:** ✅  
  VM can now save and restore full machine state to/from snapshot files on demand. Enables seamless experimentation, rollback, and resilience.

- **Multiple VM Support:** ✅  
  Run multiple virtual machines simultaneously, each with independent state and execution context.

## 🛣️ Next Steps (Upcoming Roadmap)

- **Live Migration Support:**  
  Next commit will deliver live migration capability — transfer running VM state between hosts without downtime.

- **Advanced Opcode Support:**  
  Expanding supported instructions towards device I/O, memory management, and custom VM configuration.

- **Performance Benchmarking:**  
  Testing and analysis tools to measure execution efficiency and correctness across diverse workloads.

## 🧠 Why It Matters

- **Beginner-Friendly, Research-Driven:**  
  Designed for students and curious engineers interested in the underpinnings of virtualization, step by step.

- **Customizable & Modular:**  
  Tinkering encouraged—every feature is built to be extended, hacked, and learned from.

- **Open Road to Advanced VM Features:**  
  Each increment brings the project closer to a robust, self-built hypervisor capable of snapshotting, migration, and more.

## 🛠️ Getting Started

### **Prerequisites**

1. Install [UTM](https://mac.getutm.app/) and configure your Ubuntu server image.
2. Ensure you have a C++ compiler (g++ or clang++) installed.

### **Building the Project**

Clone this repository and compile the C++ core components:

```bash
git clone https://github.com/manamsriram/baby_hypervisor.git
cd baby_hypervisor
g++ -o myvmm myvmm.cpp
```

## 🎯 Running the Hypervisor

### **Single VM Execution**

**Cold Start (without snapshot):**
```bash
./myvmm -v assembly_file_vm1.txt
```
Starts the VM from initial state. All registers and memory begin at default values.

**Warm Start (restore from snapshot):**
```bash
./myvmm -v assembly_file_vm1.txt -s snapshot_vm1.bin
```
Restores the VM from a previously saved snapshot file, resuming exactly where you left off.

### **Multiple VM Execution**

**Cold Start (multiple VMs):**
```bash
./myvmm -v assembly_file_vm1.txt -v assembly_file_vm2.txt
```
Starts multiple VMs simultaneously, each running independently from initial state.

**Warm Start (multiple VMs with snapshot):**
```bash
./myvmm -v assembly_file_vm1.txt -s snapshot_vm1.bin -v assembly_file_vm2.txt
```
Load VM1 from snapshot while starting VM2 from cold state. Mix and match snapshots as needed.

### **Snapshot Operations**

Use the 'SNAPSHOT \<filename\>' instruction to save the current registers state to the '\<filename\>'

```bash
SNAPSHOT snapshot_vm1.bin
```

This captures all registers, memory, and execution context to the specified file. Each VM can be snapshotted independently, allowing fine-grained state management across multiple virtual machines.

---

## 📢 Stay tuned for live migration support and much more in future commits!

**Have suggestions, feedback, or want to contribute? Open an issue or start a discussion!**

---
