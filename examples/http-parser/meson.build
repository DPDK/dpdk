project('http-parser', 'c')

# Find DPDK library
rte_lib = cc.find_library('rte_pmd', dirs: '/usr/local/lib')

# Define the executable
executable('http-parser', 'main.c', dependencies: rte_lib)
