import os
import subprocess

srcdir = "."
blddir = "build"

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  print ""
  print '********************************************'
  print '************** THREADS_A_GOGO **************'
  print '********************************************'
  print '**************** CONFIGURE *****************'
  print '********************************************'
  conf.check_tool("gcc")
  conf.check_tool("compiler_cxx")
  print '\n**** Checking for node_addon\n'
  conf.check_tool("node_addon")
  buildMinifier(0)

def build(bld):
  print '********************************************'
  print '************** THREADS_A_GOGO **************'
  print '********************************************'
  print '****************** BUILD *******************'
  print '********************************************'
  print '*** Minifying & C-ifying boot.js and pool.js'
  subprocess.check_call(["bash", "-c", "cd src && node js2c.js")])
  
  print '*** Building'
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall", "-O0", "-Wunused-macros"]
  obj.target = "threads_a_gogo"
  obj.source = "src/threads_a_gogo.cc"