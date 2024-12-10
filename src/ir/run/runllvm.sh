# clang -emit-llvm -S main.c -o main.ll
llvm-as main.ll -o main.bc
lli main.bc
echo $?
