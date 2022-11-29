# D2X Artifact Evaluation

## Introduction
This repository documemts the evaluation procedure for the artifacts of our CGO 2023 paper titled "D2X: An eXtensible conteXtual Debugger for modern DSLs". 
D2X (pronounced as Detox) is a library for adding debugger support to high-performance DSLs portably. This means one can add rich debugging support for their DSLs 
without any modifications (or plugins) to commonly used debuggers like gdb. The artifacts are divided into 3 parts - 
1. Testing debugging support added to popular DSL compiler - GraphIt (Section 5.1 of the paper)
2. Testing debugging support added to BuildIt DSL framework and an Einsum DSL written on top of it (Section 5.2 of the paper)
3. Learning to write a small program with BuildIt and see the debugging in action (argument for extensibility of the artifacts)

The source code of all the modified applications has been added to this repository as submodules. So you can also look at the changes to compare the results from Figure 10 and Figure 15 of the paper. 

## Hardware and Software requirements
We expect you to run the code on a Linux system (tested with Ubuntu 18.04 and Ubuntu 20.04). There are some problems with Ubuntu 22.04 and the newer version of libraries they come with.
If you do not have access to any supported systems, please contact us. We can share access to our systems. We expect the system to have around 50 GB free space. 

Following are the software requirements for all the sections - 
1. g++ >= 7.5 (comparable clang++ would also work. You might have to change the compiler in the scripts)
2. gdb >= 8.1.1 
3. make (GNU make >= 4.1)
4. cmake >= 3.10
5. bash
6. git
7. Libraries: libdwarf1, libdwarf-dev, libunwind-dev

All the software and libraries can be installed with the `apt` package manager. Please make sure all the dependencies (especially the library dependencies) are installed before you start. 


## How to run
### Cloning the repository

To start, first clone this repository using the following command -

```
git clone --recursive https://github.com/BuildIt-lang/d2x_cgo23_artifacts.git
```

If you have already cloned the repository without the recursive flag, you can run the following command inside the cloned directory to fetch all the submodules -

```
git submodule update --init --recursive
```

Now navigate to the main repostitory and continue the rest of the steps.

### Build all dependencies

We will start by building all the packages. As you can see this artifact repository has 3 submodules - the D2X library, the GraphIt DSL compiler that has been modified to support generating debugging info and the BuildIt DSL framework that has been modified to generate first stage code and variable as debugging information. 
We have provided a convenient way to build all the packages. Just run the command in the top level directory of this repository - 

```
bash build_deps.sh
```

If any of the steps fail, you can fix the issues (most likely a missing dependency) and run the script again. The script ignores dependencies that have already been built. 

### Build applications
Next we will compile all the applications we require for Section 1. and Section 2. The first is a PageRankDelta graph application written with GraphIt and the second is a Einsum DSL written with BuildIt and a simple Matrix Vector multiplication application written with the DSL. 
Once again, we have packaged all steps to build these in a script. You can run the command (in the top level directory) - 

```
bash build_apps.sh
```

If any step fails, you can fix the issue and run the script again. 

Once the applications are built, we are ready to start debugging them. We will start with Section 1. 

## Section 1: Debugging PageRankDelta generated with GraphIt. 
Please take a look at Section 5.1 of the paper if you haven't already. We will be attaching our favorite debugger `gdb` to the generated code and view the generated code, DSL input and DSL variables. 
You can start the debugger as - 

```
./gdb-wrapper --args ./build/pagerankdelta graphit/test/graphs/4.mtx
```

`gdb-wrapper` is simply a wrapper around unmodified `gdb` that loads a init file that has some convenient macros defined (like `xbt`, `xlist` etc). These are not required, but make the interface easy to use. 
You can view `gdb-wrapper` by running `cat gdb-wrapper` and the helper init file that it loads by running `cat d2x/helpers/gdb/d2x-gdb.init`. You can see that these macros are nothing but calls to the functions defined in the D2X runtime. 

Troubleshoot: If `gdb-wrapper` refuses to run with permission denied, make sure it is executable by running the command `chmod +x gdb-wrapper`. You can also skip the gdb-wrapper and invoke gdb directly with the command - 

```
gdb --command=d2x/helpers/gdb/d2x-gdb.init --args ./build/pagerankdelta graphit/test/graphs/4.mtx
```

Once you are inside the debugger, run the following commands - 

```
break main
run
```

This will run the program and stop the execution at the start of the main function. We are now ready to explore the execution. We will first demonstrate how the frontiers in the program can be printed using the `rtv_handler` that we have written. 
Start by inserting a breakpoint after the first iteration of pagerank with the commands - 

```
break 768
c
```

You will notice that the execution will stop at a line that reads `deleteObject(frontier)`. This is a line in the generated code. You can view around this code by running the usual gdb command `list`. 
We are now ready to view the extended debugging state. Run the following commands one after the other - 

```
xlist
xbt
xframe
xvars
```

Each of these commands will show you different aspects of the extended debugging state (state from the DSL). The `xlist` command will show the source listing of the original DSL. `xbt` and `xframe` behave similar to `bt` and `frame` respectively and show the extended call stack. 
The `xvars` command shows the extended variables that are live at this point. We have modified the GraphIt DSL compiler to display Vertex Sets. You should see two extended variables `frontier` and `output`.

Display these variables with the commands - 

```
xvars frontier
xvars output
```

You should be able to see the state of the frontier before and after the call. Feel free to continue the execution a couple of times with the `c` command and checking the extended variables with the same commands above. 

Once we have checked the extended variables, we will check how the UDF (User Defined Functions) capture the calling context in the DSL. 

Clear all the breakpoints with the commands - 

```
del break
```

If it prompts for confirmation, hit y. 

Now insert a new breakpoint inside the UDF using the command - 

```
break 352
```

Start the execution again using the command `run`. If the debugger prompts for confirmation, hit y. 

You will see that the execution stops inside the UDF. The line should read something like - `ngh_sum[dst] += (delta[src] / out_degree[src]);`. You can now check the extended backtrace with the command `xbt`. The top frame is the UDF and the second frame is operator that calls this UDF. 

You can view the source DSL with the `xlist` command and change frames using `xframe 1` and `xframe 0`. You can check the source code for each frame with the `xlist` command. 

Notice that the extended backtrace is not the same as the backtrace in the generated code. The generated code may or may not show the callsite. Infact the same UDF can specialized differenly for different callsites. 
Feel free to navigate and run `step` or `next` and view the code around. 

When done viewing this, exit the debugger with the `quit` command. You might need to confirm by hitting y. 


## Section 2: Debugging Einsum DSL and generated Matrix Vector multiplication written with BuildIt
In the Section 5.2 of the paper we demonstrated how D2X can be applied to the BuildIt DSL framework. Once this is done, any DSL written on top of BuildIt would get support for debugging without any change. We showed the example of an Einsum DSL in the paper. We will test the same here. 

Once again, start by running the debugger as - 

```
./gdb-wrapper --args ./build/mvp
```

Once inside the debugger, insert breakpoint at the start of matrix_vector and start with the commands - 

```
break matrix_vector
run
```

Execution will stop at the start of the matrix_vector function. We will directly jump to the point where the matrix vector multiplication happens. Insert a breakpoint and continue with - 

```
break 20
c
```

The execution will stop inside a doubly nested loop that does the reduction for Matrix Vector product. Once again view the extended stack, the extended source with the commands - 

```
xbt
xlist
xframe
```

You can see the einsum expression in the DSL by running the commands - 

```
xframe 7
xlist
```

You should see the listing like - `c[i] = 2 * a[i][j] * b[j];`. What we have done here is to show the source code and static variables from the first stage execution. Since the Einsum DSL is implemented as an embedded DSL with BuildIt, we can see the internals of the compiler and how specialization in the first stage generates the code. 

This Einsum DSL implements constant propagation by tracking tensors that are constant as static variables. You can see all these analysis variables by running the command - 

```
xvars
```

You should 6 variables. View each of them by running - 

```
xvars a.is_constant
xvars a.constant_val
xvars b.is_constant
xvars b.constant_val
xvars c.is_constant
xvars c.constant_val
```

You will see that `b` is the only tensor that is currently initalized as a constant and the value is 1. This is consistent with the fact that the DSL inserts a constant (`1`) where `b` is referenced in the einsum expression. Feel free to change the input to the DSL in the source file `einsum.cpp` in the main repo at line number - 347. If you make any changes, please recompile applications using `bash build_apps.sh` as before. 

Feel free to navigate around with `next` or `step` commands and viewing the extended state with `xlist`, `xvars`, `xframe` and `xbt`. Once you are satisfied exit the debugger with `quit`. Hit y to confirm when prompted. 