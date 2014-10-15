import subprocess

project = 'DPDK'

copyright = '2014, Intel'

version = subprocess.check_output(["make","-sRrC","../../", "showversion"])

master_doc = 'index'
