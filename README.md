# 🧑‍💻 baby_hypervisor

**A Learning Hypervisor Project — UTM Virtualization, C++ Backed, Stepwise Innovation**

---

## 🚀 Overview

baby_hypervisor is a personal exploration into **hypervisor development using C++**, designed to virtualize a custom Ubuntu server image atop the [UTM](https://mac.getutm.app/) hypervisor platform. The project builds incrementally, aiming to demystify how low-level virtualization works, and brings new capabilities with each release.

## 💡 Current Features (First Commit: v0.1)

- **Arithmetic, Logical & Numeric Operations:**  
  The initial implementation enables basic VM operation decoding for fundamental arithmetic, logical, and numeric tasks inside the guest Ubuntu instance.

- **UTM Integration:**  
  Ubuntu is deployed and managed as a VM using UTM, enabling rapid iteration and robust snapshotting for experimental development.

- **Modern C++ Foundations:**  
  VM core and opcode interpreter written in C++, with extensible design for isolation, error handling, and future expansion.

## 🛣️ Next Steps (Upcoming Roadmap)

- **Snapshot Support:**  
  Next commit will deliver VM snapshot capability — save and restore machine/guest state on demand for seamless experimentation and resilience.

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

1. Install [UTM](https://mac.getutm.app/) and configure your Ubuntu server image.
2. Clone this repo and build the C++ core components (`myvmm.cc`).
3. Launch the VM via UTM, connect to your custom manager, and interact with supported operations from the instruction set.

Example build (requires g++ or clang++)
g++ -o myvmm myvmm.cc

---

## 📢 Stay tuned for VM snapshot support and much more in future commits!

**Have suggestions, feedback, or want to contribute? Open an issue or start a discussion!**

---
