source ../../../install_dir.sh
$LLVM_BIN_INSTALL_DIR/decoup-mem ../../spmv.bc -o mem_decoup_w_dup.ll -cpu-mode -burst
