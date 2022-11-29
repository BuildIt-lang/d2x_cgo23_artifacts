# Compile Page Rank Delta with GraphIt
echo "Generating and compiling PageRank Delta with Graphit"
./graphit/build/bin/graphitc -f graphit/apps/pagerankdelta.gt -o build/pagerankdelta.cpp
g++ -g build/pagerankdelta.cpp -o build/pagerankdelta -I graphit/src/runtime_lib -I d2x/include d2x/runtime/d2x_runtime.cpp -ldwarf -lunwind -ldl


# Compiler Einsum DSL and example
echo "Generating and compiling Matrix Vector multiplication with Einsum DSL on top of BuildIt"
g++ -g einsum.cpp -o build/gen-einsum $(make --no-print-directory -C buildit compile-flags) $(make --no-print-directory -C buildit linker-flags)
./build/gen-einsum > build/mvp.cpp
g++ -g build/mvp.cpp mvp_driver.cpp -o build/mvp -I d2x/include d2x/runtime/d2x_runtime.cpp -ldwarf -lunwind -ldl
