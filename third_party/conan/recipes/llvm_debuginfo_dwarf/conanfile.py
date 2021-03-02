from conans import python_requires
import os

common = python_requires('llvm-common/0.0.3@orbitdeps/stable')

class LLVMDebugInfoDWARF(common.LLVMModulePackage):
    version = common.LLVMModulePackage.version
    name = 'llvm_debuginfo_dwarf'
    llvm_component = 'llvm'
    llvm_module = 'DebugInfoDWARF'
    llvm_requires = ['llvm_headers', 'llvm_object', 'llvm_support',
                     'llvm_binary_format', 'llvm_mc']
