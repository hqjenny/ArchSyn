# ArchSyn README
The **ArchSyn** code is developed on LLVM 3.6.2.

## To install it: 
1. Clone llvm source to local repository 
- `git clone -b release_36 https://github.com/llvm-mirror/llvm.git`
2. Copy the code over to the llvm repository
- `cp -r <ARCHSYN_ROOT>/ArchSyn/ <LLVM_ROOT>/llvm/`
3. Recompile the llvm repository, please refer to  
https://llvm.org/docs/CMake.html 
You should see the executables: gen-dpp, gen-par, gen-sync and decoup-mem, under the /bin folder 

## To run it: 
We have a sample design under ArchSyn/archsyn_regression.  
1. `cd <ARCHSYN_ROOT>/ArchSyn/archsyn_regression`
2. Edit the path in install_dir.sh to your LLVM bin folder 
3. The source code is archsyn_regression/spmv/spmv.cpp, to compile it 
- `bash regen.sh`
4. Generate the C for synthesis, 
- `cd hls/dpp`
- `bash gen.sh`
5. `cp ../../../../scripts/parse.pl . ` 
6. Run the parser `perl parse.pl <TOP_FUNC_NAME> <PATH_TO_SCRIPTS>`.
For this example run `perl parse.pl spmv <ARCHSYN_ROOT>/ArchSyn/scripts`
7. Run Vivado, 
`cd vivado_hls/spmv`
`vivado -source run_hls.tcl`


