# Odd-even sort
Parallel implementation of the odd-even sort algorithm.

The project has been developed and tested on the Intel Xeon Phi 7230 (64 cores), heavily relying on the 512-bit vectorization.

## Results

Speedup of the native-threads version:
![](https://github.com/michelezoncheddu/parallel-odd-even-sort/blob/main/doc/img/native.png?raw=true)

Speedup of the FastFlow version:
![](https://github.com/michelezoncheddu/parallel-odd-even-sort/blob/main/doc/img/ff.png?raw=true)
